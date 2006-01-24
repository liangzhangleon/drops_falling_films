//**************************************************************************
// File:    ipdrops.cpp                                                    *
// Content: test program for the instat. poisson-problem                   *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, Marcus Soemers      *
//          IGPM RWTH Aachen                                               *
// Version: 0.1                                                            *
// History: begin - Nov, 19 2002                                           *
//**************************************************************************

#include "poisson/instatpoisson.h"
#include "poisson/integrTime.h"
#include "geom/multigrid.h"
#include "num/solver.h"
#include "num/MGsolver.h"


// du/dt - nu*laplace u + Vel grad u + q*u = f

class PoissonCoeffCL
{
  public:
    static double q(const DROPS::Point3DCL&) { return 0.0; }
    //static double f(const DROPS::Point3DCL& , double ) { return 0.0; }
    static double f(const DROPS::Point3DCL& p, double t)
      { return (-2.0*exp(t)*exp(p[0]+p[1]+p[2])); }
    static DROPS::Point3DCL Vel(const DROPS::Point3DCL&, double)
      { return DROPS::Point3DCL(0.); } // no convection
};


//inline double Lsg(const DROPS::Point3DCL& , double ) { return 38.0; }
inline double Lsg(const DROPS::Point3DCL& p, double t)
  { return (exp(t)*exp(p[0]+p[1]+p[2])); }


// boundary functions (neumann, dirichlet type)
/*
inline double GradX(const DROPS::Point3DCL& p, double t)
  { return (4.0); }
inline double GradY(const DROPS::Point3DCL& p, double t)
  { return (0.0); }
inline double GradZ(const DROPS::Point3DCL& p, double t)
  { return (0.0); }
*/  

inline double GradX(const DROPS::Point3DCL& p, double t)
  { return (exp(t)*exp(p[0]+p[1]+p[2])); }
inline double GradY(const DROPS::Point3DCL& p, double t)
  { return (exp(t)*exp(p[0]+p[1]+p[2])); }
inline double GradZ(const DROPS::Point3DCL& p, double t)
  { return (exp(t)*exp(p[0]+p[1]+p[2])); }
/*
inline double GradX(const DROPS::Point3DCL& p, double t)
{
  if (p[0]==0)
    return 6e-3;
  else
    return (1e-3*sin((p[2]/30)*2*M_PI+(t/190)*2*M_PI));
}
inline double GradY(const DROPS::Point3DCL& p, double t) { return 0.0; }
inline double GradZ(const DROPS::Point3DCL& p, double t) { return 0.0; }
*/

namespace DROPS 
{

class DirValCL
{
  private:
    static MultiGridCL* _mg;
    
  public:
    void SetMG( MultiGridCL* mg) { _mg= mg; }
    template<int seg> static double dir_val(const DROPS::Point2DCL& p, double t)
      { return Lsg(_mg->GetBnd().GetBndSeg(seg)->Map(p), t); }
};

class NeuValCL
{
  private:
    static MultiGridCL* _mg;
    
  public:
    void SetMG( MultiGridCL* mg) { _mg= mg; }
    template<int seg> static double neu_val(const DROPS::Point2DCL& p, double t)
    { 
      switch (seg)
      {
        case 0:
          return (-GradX(_mg->GetBnd().GetBndSeg(seg)->Map(p), t));
        case 1:
          return GradX(_mg->GetBnd().GetBndSeg(seg)->Map(p), t);
        case 2:
          return (-GradY(_mg->GetBnd().GetBndSeg(seg)->Map(p), t));
        case 3:
          return GradY(_mg->GetBnd().GetBndSeg(seg)->Map(p), t);
        case 4:
          return (-GradZ(_mg->GetBnd().GetBndSeg(seg)->Map(p), t));
        case 5:
          return GradZ(_mg->GetBnd().GetBndSeg(seg)->Map(p), t);
        default:
        {
          std::cerr <<"error: neu_val";
          return 1;
        }
      }
    }
};

MultiGridCL* DirValCL::_mg= NULL;
MultiGridCL* NeuValCL::_mg= NULL;

template<class Coeff>
void MGStrategy(InstatPoissonP1CL<Coeff>& Poisson, double dt, double time_steps,
  double nu, double theta, double tol, int maxiter)
{

  MultiGridCL& MG= Poisson.GetMG();
  IdxDescCL* c_idx;
  MGDataCL MGData;

  // Initialize the MGData: Idx, A, P, R
  for(Uint lvl= 0; lvl<=MG.GetLastLevel(); ++lvl)
  {
    MGData.push_back(MGLevelDataCL());
    MGLevelDataCL& tmp= MGData.back();
    std::cerr << "Create MGData on Level " << lvl << std::endl;
    tmp.Idx.Set(1);
    Poisson.CreateNumbering(lvl, &tmp.Idx);
    MatDescCL& A= Poisson.A;
    MatDescCL& M= Poisson.M;
    A.SetIdx(&tmp.Idx, &tmp.Idx);
    M.SetIdx(&tmp.Idx, &tmp.Idx);
    tmp.A.SetIdx(&tmp.Idx, &tmp.Idx);

    if(lvl==MG.GetLastLevel())
    {
      Poisson.x.SetIdx(&tmp.Idx);
      Poisson.b.SetIdx(&tmp.Idx);
      std::cerr << "Create System " << std::endl;
      Poisson.SetupInstatSystem(A, M);
      tmp.A.Data.LinComb(1., M.Data, theta*dt*nu, A.Data);
    }
    else
    {
      std::cerr << "Create System" << std::endl;
      Poisson.SetupInstatSystem(A, M);
      tmp.A.Data.LinComb(1., M.Data, theta*dt*nu, A.Data);
    }
    if(lvl!=0)
    {
      std::cerr << "Create Prolongation on Level " << lvl << std::endl;
      Poisson.SetupProlongation(tmp.P, c_idx, &tmp.Idx);
    }
    c_idx= &tmp.Idx;
  }
  std::cerr << "Check Data...\n";
  CheckMGData(MGData.begin(), MGData.end());

  MGSolverCL mg_solver(MGData, maxiter, tol);
  InstatPoissonThetaSchemeCL<InstatPoissonP1CL<Coeff>, MGSolverCL>
    ThetaScheme(Poisson, mg_solver, theta);

  ThetaScheme.SetTimeStep(dt);

  scalar_instat_fun_ptr exact_sol = &Lsg;
  
  // ****** Startwert
  
  typedef std::pair<double, double> d_pair;
  typedef std::pair<double, d_pair> cmp_key;
  typedef std::map<cmp_key, double*> node_map;
  typedef node_map::const_iterator ci;
  
  Point3DCL pt;
  VecDescCL& x= Poisson.x;
  Uint lvl= x.GetLevel();
  Uint indx= x.RowIdx->GetIdx();
    
  d_pair help;
  cmp_key key;
  node_map nmap;
    
  for (MultiGridCL::TriangVertexIteratorCL sit=MG.GetTriangVertexBegin(lvl), 
    send=MG.GetTriangVertexEnd(lvl); sit != send; ++sit)
  { 
    if (sit->Unknowns.Exist())
    {
      IdxT i= sit->Unknowns(indx);
      pt= sit->GetCoord();
    
      help= std::make_pair(pt[2], pt[1]);
      key= std::make_pair(pt[0], help);
      x.Data[i]= Lsg(pt, 0.0);
      nmap[key]= &(x.Data[i]);
    }
  }
  
  
  // Ausgabe Startwert
  
  //for (ci p= nmap.begin(); p!= nmap.end(); p++)
  //{
  //  std::cerr << *(p->second) << "\n";
  //}
  
 
  // ****** Ende Startwert
  
  double average= 0.0;
  for (int step=1;step<=time_steps;step++)
  {
    ThetaScheme.DoStep(x);
    std::cerr << "t= " << Poisson.t << std::endl;
    std::cerr << "Iterationen: " << mg_solver.GetIter() 
      << "    Norm des Residuums: " << mg_solver.GetResid() << std::endl;
    Poisson.CheckSolution(x, exact_sol, Poisson.t);
    average+= mg_solver.GetIter();
  }
  average/= time_steps;
  std::cerr << "Anzahl der Iterationen im Durchschnitt: " << average
    << std::endl;
  
  /*
  // Ausgabe Loesung   
  
  for (ci p= nmap.begin(); p!= nmap.end(); p++)
  {
    std::cerr << *(p->second) << "\n";
  }
  */
  
}

template<class Coeff>
void CGStrategy(InstatPoissonP1CL<Coeff>& Poisson, double dt, double time_steps,
  double nu, double theta, double tol, int maxiter)
{
  
  MultiGridCL& MG= Poisson.GetMG();
  IdxDescCL& idx= Poisson.idx;
  VecDescCL& x= Poisson.x;
  VecDescCL& b= Poisson.b;
  MatDescCL& A= Poisson.A;
  MatDescCL& M= Poisson.M;

  idx.Set(1);
  Poisson.CreateNumbering(MG.GetLastLevel(), &idx);

  x.SetIdx(&idx); 
  b.SetIdx(&idx);
  A.SetIdx(&idx, &idx);
  M.SetIdx(&idx, &idx);

  std::cerr << "Anzahl der Unbekannten: " <<  Poisson.x.Data.size()
    << std::endl;
  Poisson.SetupInstatSystem(A, M);
  
  SSORPcCL pc(1.0);
  PCG_SsorCL pcg_solver(pc, maxiter, tol);
  InstatPoissonThetaSchemeCL<InstatPoissonP1CL<Coeff>, PCG_SsorCL>
    ThetaScheme(Poisson, pcg_solver, theta);

  ThetaScheme.SetTimeStep(dt, nu);

  scalar_instat_fun_ptr exact_sol = &Lsg;
  
  // ****** Startwert
  
  typedef std::pair<double, double> d_pair;
  typedef std::pair<double, d_pair> cmp_key;
  typedef std::map<cmp_key, double*> node_map;
  typedef node_map::const_iterator ci;
  
  Point3DCL pt;
  Uint lvl= x.GetLevel();
  Uint indx= x.RowIdx->GetIdx();
    
  d_pair help;
  cmp_key key;
  node_map nmap;
    
  for (MultiGridCL::TriangVertexIteratorCL sit=MG.GetTriangVertexBegin(lvl), 
    send=MG.GetTriangVertexEnd(lvl); sit != send; ++sit)
  { 
    if (sit->Unknowns.Exist())
    {
      IdxT i= sit->Unknowns(indx);
      pt= sit->GetCoord();
    
      help= std::make_pair(pt[2], pt[1]);
      key= std::make_pair(pt[0], help);
      x.Data[i]= Lsg(pt, 0.0);
      nmap[key]= &(x.Data[i]);
    }
  }
  
  
  // Ausgabe Startwert
  
  //for (ci p= nmap.begin(); p!= nmap.end(); p++)
  //{
  //  std::cerr << *(p->second) << "\n";
  //}
  
 
  // ****** Ende Startwert
  
  double average= 0.0;
  for (int step=1;step<=time_steps;step++)
  {
    ThetaScheme.DoStep(x);
    std::cerr << "t= " << Poisson.t << std::endl;
    std::cerr << "Iterationen: " << pcg_solver.GetIter() 
      << "    Norm des Residuums: " << pcg_solver.GetResid() << std::endl;
    Poisson.CheckSolution(x, exact_sol, Poisson.t);
    average+= pcg_solver.GetIter();
  }
  average/= time_steps;
  std::cerr << "Anzahl der Iterationen im Durchschnitt: " << average
    << std::endl;
  /*
  // Ausgabe Loesung   
  
  for (ci p= nmap.begin(); p!= nmap.end(); p++)
  {
    std::cerr << *(p->second) << "\n";
  }
  */
 
}

} // end of namespace DROPS



int main()
{
  try
  {
    DROPS::Point3DCL null(0.0);
    DROPS::Point3DCL e1(0.0), e2(0.0), e3(0.0);
    e1[0]= 1;
    e2[1]= 1;
    e3[2]= 1;

    typedef DROPS::InstatPoissonP1CL<PoissonCoeffCL> 
      InstatPoissonOnBrickCL;
    typedef InstatPoissonOnBrickCL MyPoissonCL;
    
    DROPS::BrickBuilderCL brick(null, e1, e2, e3, 2, 2, 2);
  
    double dt= 0.0;
    int time_steps= 0, brick_ref= 0;
    
    //dt= 0.000666;
    //time_steps= 190;
    //dt= 0.01;
    //time_steps= 100;
    //brick_ref= 3;
    std::cerr << "\nDelta t = "; std::cin >> dt;
    std::cerr << "\nAnzahl der Zeitschritte = "; std::cin >> time_steps;
    std::cerr << "\nAnzahl der Verfeinerungen = "; std::cin >> brick_ref;

    /*
    // Dirichlet boundary conditions 
    const bool isneumann[6]= 
      { false, false, false, false, false, false };
    const DROPS::InstatPoissonBndDataCL::bnd_val_fun bnd_fun[6]=
      { &DROPS::DirValCL::dir_val<0>, &DROPS::DirValCL::dir_val<1>,
        &DROPS::DirValCL::dir_val<2>, &DROPS::DirValCL::dir_val<3>,
        &DROPS::DirValCL::dir_val<4>, &DROPS::DirValCL::dir_val<5> };
    */

    // Neumann boundary conditions 
    const bool isneumann[6]= 
      { true, true, true, true, true, true };
    const DROPS::InstatPoissonBndDataCL::bnd_val_fun bnd_fun[6]=
      { &DROPS::NeuValCL::neu_val<0>, &DROPS::NeuValCL::neu_val<1>,
        &DROPS::NeuValCL::neu_val<2>, &DROPS::NeuValCL::neu_val<3>,
        &DROPS::NeuValCL::neu_val<4>, &DROPS::NeuValCL::neu_val<5> };
 
      
    DROPS::InstatPoissonBndDataCL bdata(6, isneumann, bnd_fun);
    MyPoissonCL prob(brick, PoissonCoeffCL(), bdata);
    DROPS::MultiGridCL& mg = prob.GetMG();
    
    DROPS::DirValCL dvd;
    DROPS::NeuValCL dvn;
    dvd.SetMG( &mg);
    dvn.SetMG( &mg);
    
    for (int count=1; count<=brick_ref; count++)
    {
      MarkAll(mg);
      mg.Refine();
    }
    mg.SizeInfo(std::cerr);

    // Diffusionskoeffizient
    //double nu= 6.303096;
    double nu= 1;
    
    // one-step-theta scheme 
    double theta= 0.5;
  
    // Daten fuer den Loeser
    double tol= 1.0e-7;
    int maxiter= 5000; 

    DROPS::CGStrategy(prob, dt, time_steps, nu, theta, tol, maxiter);
    //DROPS::MGStrategy(prob, dt, time_steps, nu, theta, tol, maxiter);
   
    return 0;
  }
  catch (DROPS::DROPSErrCL err) { err.handle(); }
  
}



