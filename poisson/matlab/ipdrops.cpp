//**************************************************************************
// File:    ipdrops.cpp                                                    *
// Content: program with interface for matlab                              *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, Marcus Soemers      *
//          IGPM RWTH Aachen                                               *
// Version: 0.1                                                            *
// History: begin - Nov, 19 2002                                           *
//**************************************************************************

#include "mex.h"

#include "../instatpoisson.h"
#include "../../num/solver.h"
#include "../integrTime.h"
#include "../../out/output.h"
#include <fstream>


// Das folgende Problem wird untersucht
// du/dt - nu * laplace(u) + q*u = f


extern void _main();


// Setzen der Koeffizienten
class PoissonCoeffCL
{
  private:
    static int _Flag;	
    
  public:
    PoissonCoeffCL(int Flag) { _Flag= Flag; }
    static double q(const DROPS::Point3DCL&) { return 0.0; }
    static double f(const DROPS::Point3DCL& p, double t)
    {
      if (_Flag==0)
        return 0.0;
      else
        return 0.0; 
    }
};

int PoissonCoeffCL::_Flag= 0;

// Die Klasse MatConnect bildet das Interface zwischen
// C++ und Matlab

class MatConnect
{
  private:
    typedef std::pair<double, double> d_pair;
    typedef std::pair<double, d_pair> cmp_key;
    typedef std::map<cmp_key, double*> node_map;
    typedef node_map::const_iterator ci;
  
    static node_map _NodeMap;
    static double* _BndData[6];
    static double* _T0;
    static double _DeltaT, _XLen, _YLen, _ZLen, _SpIncrX, _SpIncrY, _SpIncrZ;
    static int _MeshRefX, _MeshRefY, _MeshRefZ, _Count, _CutPos, _FacePtsYZ;
	
  public:
    MatConnect(double DeltaT, 
      double xl, double yl, double zl,
      int mrx, int mry, int mrz,
      int CutPos, int FacePtsYZ,  
      double* T0,
      double* S1, double* S2, double* S3,
      double* S4, double* S5, double* S6)
    {
      _NodeMap.clear();
      _DeltaT= DeltaT;
      _XLen= xl; _YLen= yl; _ZLen= zl;
      _MeshRefX= mrx; _MeshRefY= mry; _MeshRefZ= mrz;
      _SpIncrX= xl/mrx; _SpIncrY= yl/mry; _SpIncrZ= zl/mrz;
      _Count= 0; _CutPos= CutPos; _FacePtsYZ= FacePtsYZ;
      _T0= T0;
      _BndData[0]= S1; _BndData[1]= S2; _BndData[2]= S3;
      _BndData[3]= S4; _BndData[4]= S5; _BndData[5]= S6;
    }
    
    template<int num> static int getLexNum(const DROPS::Point2DCL& p)
    {
      int count= 0;
      if (num==0)  // Punkt liegt auf der Seite S1 oder S2
      {
        for (double zcoord= .0; zcoord<= _ZLen*p[1]-_SpIncrZ/2.0; zcoord+= _SpIncrZ)
          count+= (_MeshRefY+1);
        for (double ycoord= .0; ycoord<= _YLen*p[0]+_SpIncrY/2.0; ycoord+= _SpIncrY)
          count++;
      }
      else if (num==1)  // Punkt liegt auf einer der Seiten S3 oder S4
      {
        for (double zcoord= .0; zcoord<= _ZLen*p[1]-_SpIncrZ/2.0; zcoord+= _SpIncrZ)
          count+= (_MeshRefX+1);
        for (double xcoord= .0; xcoord<= _XLen*p[0]+_SpIncrX/2.0; xcoord+= _SpIncrX)
          count++;
      }
      else  // Punkt liegt auf einer der Seiten S5 oder S6
      {
        for (double ycoord= .0; ycoord<= _YLen*p[1]-_SpIncrY/2.0; ycoord+= _SpIncrY)
          count+= (_MeshRefX+1);
        for (double xcoord= .0; xcoord<= _XLen*p[0]+_SpIncrX/2.0; xcoord+= _SpIncrX)
          count++;
      }
    
      return count;
    }
  
    template<int num> static double getBndVal(const DROPS::Point2DCL& p, double t)
    {
      int count= -1;  // das Feld beginnt mit Index 0
    
      // die Matrizen S1..S6 werden spaltenweise uebergeben
      if (num==0 || num==1)  // Seite S1 oder S2
      {
        for (double time= .0; time< t-_DeltaT/2.0; time+= _DeltaT)
          count+= (_MeshRefY+1)*(_MeshRefZ+1);
        count+= getLexNum<0>(p);
      }
      else if (num==2 || num==3) // Seite S3 oder S4
      {
        for (double time= .0; time< t-_DeltaT/2.0; time+= _DeltaT)
          count+= (_MeshRefX+1)*(_MeshRefZ+1);
        count+= getLexNum<1>(p);
      }
      else  // Seite S5 oder S6
      {
        for (double time= .0; time< t-_DeltaT/2.0; time+= _DeltaT)
          count+= (_MeshRefX+1)*(_MeshRefY+1);
        count+= getLexNum<2>(p);
      }
    
      return *(_BndData[num]+count);
    }
    
    static void setNodeMap(DROPS::VecDescCL& x, DROPS::MultiGridCL& MG)
    {
      DROPS::Point3DCL pt;
      DROPS::Uint lvl= x.RowIdx->TriangLevel;
      DROPS::Uint indx= x.RowIdx->GetIdx();
  
      d_pair help;
      cmp_key key;
            
      for (DROPS::MultiGridCL::TriangVertexIteratorCL sit=MG.GetTriangVertexBegin(lvl),
        send=MG.GetTriangVertexEnd(lvl); sit != send; ++sit)
      {
        DROPS::IdxT i= sit->Unknowns(indx);
        pt= sit->GetCoord();
    
        help= std::make_pair(pt[2], pt[1]);
        key= std::make_pair(pt[0], help);
        _NodeMap[key]= &(x.Data[i]);
      }
    }
    
    static void setInitialData()
    {
      int count;
      double xc, yc, zc;
      
      for (ci p= _NodeMap.begin(); p!= _NodeMap.end(); p++)
      {
        count= 0;
        
        xc= p->first.first;
        yc= p->first.second.second;
        zc= p->first.second.first;
        
        int a= static_cast<int>((_MeshRefX*xc/_XLen)+0.5);
        int b= static_cast<int>((_MeshRefY*yc/_YLen)+0.5);
        int c= static_cast<int>((_MeshRefZ*zc/_ZLen)+0.5);
      
        for (int j=0; j<a; j++)
          count+= _FacePtsYZ;
        for (int k=0; k<c; k++)
          count+= _MeshRefY+1;
        count+= b;
        
        //mexPrintf("xc, yc, zc, uh: %g %g %g %g\n", xc, yc, zc, *(_T0+count));
        //mexPrintf("a, b, c: %d %d %d\n", a, b, c);
        //mexPrintf("count: %d\n", count);
        
        *(p->second)= *(_T0+count);
      }
    }
    
    static void setOutputData(double* sol2D)
    {
      ci p= _NodeMap.begin();
      for (int i=0; i<_CutPos; i++)
        for (int j=0; j<_FacePtsYZ; j++)
          if (p!=_NodeMap.end())
            p++;
      for (int k=0; k<_FacePtsYZ; k++)
      {
        *(sol2D+_Count)= *(p->second);
        p++;
        _Count++;
      }
    }
        
    static void printData()
    {
      mexPrintf("DeltaT: %g\n", _DeltaT);
      mexPrintf("XLen: %g\n", _XLen);
      mexPrintf("YLen: %g\n", _YLen);
      mexPrintf("ZLen: %g\n", _ZLen);
      mexPrintf("SpIncrX: %g\n", _SpIncrX);
      mexPrintf("SpIncrY: %g\n", _SpIncrY);
      mexPrintf("SpIncrZ: %g\n", _SpIncrZ);
      mexPrintf("MeshRefX: %d\n", _MeshRefX);
      mexPrintf("MeshRefY: %d\n", _MeshRefY);
      mexPrintf("MeshRefZ: %d\n", _MeshRefZ);
      mexPrintf("Count: %d\n", _Count);
      mexPrintf("CutPos: %d\n", _CutPos);
      mexPrintf("FacePtsYZ: %d\n", _FacePtsYZ);

      for (ci p=_NodeMap.begin(); p!=_NodeMap.end(); p++)
        mexPrintf("xc yc zc uh: %g %g %g %g\n", p->first.first, p->first.second.second,
          p->first.second.first, *(p->second));
    }
    
    
    
};

MatConnect::node_map MatConnect::_NodeMap;
double* MatConnect::_BndData[6];
double* MatConnect::_T0;
double MatConnect::_DeltaT= .0;
double MatConnect::_XLen= .0;
double MatConnect::_YLen= .0;
double MatConnect::_ZLen= .0;
double MatConnect::_SpIncrX= .0;
double MatConnect::_SpIncrY= .0;
double MatConnect::_SpIncrZ= .0;
int MatConnect::_MeshRefX= 0;
int MatConnect::_MeshRefY= 0;
int MatConnect::_MeshRefZ= 0;
int MatConnect::_Count= 0;
int MatConnect::_CutPos= 0;
int MatConnect::_FacePtsYZ= 0;



namespace DROPS // for Strategy
{

double getIsolatedBndVal(const Point2DCL& p, double t) { return 0.0; };

/*
void MarkBndTetrahedra(MultiGridCL& mg, Uint maxLevel)
{
    for (MultiGridCL::TriangTetraIteratorCL It(mg.GetTriangTetraBegin(maxLevel)),
             ItEnd(mg.GetTriangTetraEnd(maxLevel)); It!=ItEnd; ++It)
    {
      for(int i=0; i<4; ++i)
        if ((It->IsBndSeg(i)) && (It->GetBndIdx(i)==0 || It->GetBndIdx(i)==1))
          It->SetRegRefMark();
    }
}
*/

/*
void MarkBndTetrahedra(MultiGridCL& mg, Uint maxLevel)
{
    for (MultiGridCL::TriangTetraIteratorCL It(mg.GetTriangTetraBegin(maxLevel)),
             ItEnd(mg.GetTriangTetraEnd(maxLevel)); It!=ItEnd; ++It)
    {
      for(int i=0; i<4; ++i)
        if (It->GetVertex(i)->IsOnBoundary())
          for (VertexCL::const_BndVertIt VertIter=It->GetVertex(i)->GetBndVertBegin();
            VertIter!=It->GetVertex(i)->GetBndVertEnd(); ++VertIter)
              if (VertIter->GetBndIdx()==0 || VertIter->GetBndIdx()==1)
              {
                It->SetRegRefMark();
                goto mark;
              }
      mark:;
    }
}
*/

/*
void MarkBndTetrahedra(MultiGridCL& mg, Uint maxLevel, double xl)
{
  Point3DCL TetraCenter(0.0);
  double width= 5.0;
  
  for (MultiGridCL::TriangTetraIteratorCL It(mg.GetTriangTetraBegin(maxLevel)),
    ItEnd(mg.GetTriangTetraEnd(maxLevel)); It!=ItEnd; ++It)
  {
    TetraCenter= GetBaryCenter(*It);
    if (fabs(TetraCenter[0])<=width || fabs(TetraCenter[0]-xl)<=width)
      It->SetRegRefMark();
  }
}
*/

void MarkBndTetrahedra(MultiGridCL& mg, Uint maxLevel, double xl)
{
  Point3DCL VertCoord(0.0);
  double width= 0.001;
  
  for (MultiGridCL::TriangTetraIteratorCL It(mg.GetTriangTetraBegin(maxLevel)),
    ItEnd(mg.GetTriangTetraEnd(maxLevel)); It!=ItEnd; ++It)
  {
    for(int i=0; i<4; ++i)
    {
      VertCoord=It->GetVertex(i)->GetCoord();
      if (fabs(VertCoord[0])<=width || fabs(VertCoord[0]-xl)<=width)
      {
        It->SetRegRefMark();
        goto mark;
      }
      mark:;
    }
  }
}


template<class Coeff>
void Strategy(InstatPoissonP1CL<Coeff>& Poisson, double* sol2D, 
  double nu, double dt, int time_steps, double theta, double cgtol, int cgiter, MatConnect* MatCon)
{
  typedef InstatPoissonP1CL<Coeff> MyPoissonCL;
  
  IdxDescCL& idx= Poisson.idx;
  VecDescCL& x= Poisson.x;
  VecDescCL& b= Poisson.b;
  MatDescCL& A= Poisson.A;
  MatDescCL& M= Poisson.M;
  
  VecDescCL cplA;
  VecDescCL cplM;
  
  // aktueller Zeitpunkt
  double t= 0;
   
  idx.Set(1, 0, 0, 0);
  
  MultiGridCL& MG= Poisson.GetMG();
  
  // erzeuge Nummerierung zu diesem Index
  Poisson.CreateNumbering(MG.GetLastLevel(), &idx);
  
  // Vektoren mit Index idx
  b.SetIdx(&idx);
  x.SetIdx(&idx);
  cplA.SetIdx(&idx);
  cplM.SetIdx(&idx);
  
  mexPrintf("Anzahl der Unbekannten: %d\n", x.Data.size());
  mexPrintf("Theta: %g\n", theta);
  mexPrintf("Toleranz CG: %g\n", cgtol);
  mexPrintf("max. Anzahl CG-Iterationen: %d\n", cgiter);
  
  // Steifigkeitsmatrix mit Index idx (Zeilen und Spalten)
  A.SetIdx(&idx, &idx);
  // Massematrix mit Index idx (Zeilen und Spalten)
  M.SetIdx(&idx, &idx);
  
  // stationaerer Anteil
  Poisson.SetupInstatSystem(A, M);
  
  // instationaere rechte Seite
  Poisson.SetupInstatRhs(cplA, cplM, t, b, t);
  
  // PCG-Verfahren mit SSOR-Vorkonditionierer
  SSORPcCL pc(1.0);
  PCG_SsorCL pcg_solver(pc, cgiter, cgtol);
  
  // Zeitdiskretisierung mit one-step-theta-scheme
  // theta=1 -> impl. Euler
  // theta=0.5 -> Crank-Nicholson
  InstatPoissonThetaSchemeCL<InstatPoissonP1CL<Coeff>, PCG_SsorCL>
    ThetaScheme(Poisson, pcg_solver, theta);
  ThetaScheme.SetTimeStep(dt, nu);
  
  MatCon->setNodeMap(x, MG);
  MatCon->setInitialData();
      
  for (int step=1;step<=time_steps;step++)
  {
    ThetaScheme.DoStep(x);
    //mexPrintf("t= %g\n", Poisson.t);
    //mexPrintf("Iterationen: %d", pcg_solver.GetIter());
    //mexPrintf("    Norm des Residuums: %g\n", pcg_solver.GetResid());
    //Poisson.CheckSolution(exact_sol, Poisson.t);
    
    MatCon->setOutputData(sol2D);
  }
  
  //MatCon->printData();
  
  A.Reset();
  b.Reset();  
}

} // end of namespace DROPS


static
void ipdrops(double* sol2D, double* T0, double* S1, double* S2, 
  double M, double xl, double yl, double zl, double nu, double mrx, 
  double mry, double mrz, double dt, int time_steps, double theta, 
  double cgtol, double cgiter, double Flag, double BndRef)
{
  try
  {
    //DROPS::TimerCL time;
    //time.Reset();
  
    DROPS::Point3DCL null(0.0);
    DROPS::Point3DCL e1(0.0), e2(0.0), e3(0.0);
    e1[0]= xl;
    e2[1]= yl;
    e3[2]= zl;
    
    typedef DROPS::InstatPoissonP1CL<PoissonCoeffCL>
      InstatPoissonOnBrickCL;
    typedef InstatPoissonOnBrickCL MyPoissonCL;
    
    int imrx= static_cast<int>(mrx+0.5);
    int imry= static_cast<int>(mry+0.5);
    int imrz= static_cast<int>(mrz+0.5);
    int iM= static_cast<int>(M+0.5);
    int icgiter= static_cast<int>(cgiter+0.5);
    int iFlag= static_cast<int>(Flag+0.5);
    int iBndRef= static_cast<int>(BndRef+0.5);
    
    int MeshRefX= imrx<<iBndRef;
    int MeshRefY= imry<<iBndRef;
    int MeshRefZ= imrz<<iBndRef;
    int FacePtsYZ= (MeshRefY+1)*(MeshRefZ+1);
    
    //mexPrintf("imrx = %d\n", imrx);
    //mexPrintf("imry = %d\n", imry);
    //mexPrintf("imrz = %d\n", imrz);  
    //mexPrintf("iM = %d\n", iM);
    //mexPrintf("icgiter = %d\n", icgiter);
    //mexPrintf("iFlag = %d\n", iFlag);
    //mexPrintf("iBndRef = %d\n", iBndRef);
    //mexPrintf("MeshRefX = %d\n", MeshRefX);
    //mexPrintf("MeshRefY = %d\n", MeshRefY);
    //mexPrintf("MeshRefZ = %d\n", MeshRefZ);
    //mexPrintf("FacePtsYZ = %d\n", FacePtsYZ);
    
    DROPS::BrickBuilderCL brick(null, e1, e2, e3, imrx, imry, imrz);
    
    mexPrintf("\nRueckgabe der Daten fuer die Flaeche x=%g\n", 
      M*xl/imrx);
    mexPrintf("\nDelta t = %g", dt);
    mexPrintf("\nAnzahl der Zeitschritte = %d\n", time_steps);
    
    // Randdaten 
    int DiscPtsXY= (MeshRefX+1)*(MeshRefY+1)*(time_steps+1);
    int DiscPtsXZ= (MeshRefX+1)*(MeshRefZ+1)*(time_steps+1);
    DROPS::VectorCL VecXY(DiscPtsXY);
    DROPS::VectorCL VecXZ(DiscPtsXZ);
    double* Sy= &VecXY[0];
    double* Sz= &VecXZ[0];
    
    MatConnect MatCon(dt, xl, yl, zl, MeshRefX, MeshRefY, MeshRefZ, iM, FacePtsYZ, 
      T0, S1, S2, Sy, Sy, Sz, Sz);  
    
    const bool isneumann[6]= { true, true, true, true, true, true };
    /*
    const DROPS::InstatPoissonBndDataCL::bnd_val_fun bnd_fun[6]=
      { &MatConnect::getBndVal<0>, &MatConnect::getBndVal<1>,
        &MatConnect::getBndVal<2>, &MatConnect::getBndVal<3>,
        &MatConnect::getBndVal<4>, &MatConnect::getBndVal<5> };
    */
    const DROPS::InstatPoissonBndDataCL::bnd_val_fun bnd_fun[6]=
      { &MatConnect::getBndVal<0>, &MatConnect::getBndVal<1>,
        &DROPS::getIsolatedBndVal, &DROPS::getIsolatedBndVal,
        &DROPS::getIsolatedBndVal, &DROPS::getIsolatedBndVal };
    
    DROPS::InstatPoissonBndDataCL bdata(6, isneumann, bnd_fun);
    MyPoissonCL prob(brick, PoissonCoeffCL(iFlag), bdata);
    DROPS::MultiGridCL& mg = prob.GetMG();
    
    for (int ref=0; ref<iBndRef; ref++)
    {
      DROPS::MarkBndTetrahedra(mg, mg.GetLastLevel(), xl);
      mg.Refine();
    }
    
    // mg.SizeInfo();
    DROPS::Strategy(prob, sol2D, nu, dt, time_steps, theta, cgtol, icgiter, &MatCon);
    
    //time.Stop();
    //mexPrintf("Zeit fuer das Loesen des direkten Problems: %g sek\n", time.GetTime());
    
    std::ofstream fil("ttt.off");
    fil << DROPS::GeomMGOutCL(mg, -1, true, 0.0) << std::endl;
    
    return;
  }
  catch (DROPS::DROPSErrCL err) { err.handle(); }

}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  double nu, dt, xl, yl, zl, M, mrx, mry, mrz, CGTol, CGIter, theta, Flag, BndRef;
  double *T0, *S1, *S2, *sol2D;
  int mrows, ncols;
  
  /* Check for proper number of arguments. */
  if(nrhs!=17)
    mexErrMsgTxt("(T0, S1, S2, M, xl, yl, zl, nu, mrx, mry, mrz, dt, Theta, CGTol, CGIter, Flag, BndRef) as input required.");
  if(nlhs!=1)
    mexErrMsgTxt("Solution on 2D-area as output required.");
  
  /* Check to make sure the first input arguments are double matrices. */
  for (int index=0; index<3; index++)
    if(mxGetPi(prhs[index])!=NULL)
    {
      switch(index) {
      case 0:
        mexErrMsgTxt("Input T0 must be a double matrix.");
      case 1:
        mexErrMsgTxt("Input S1 must be a double matrix.");
      case 2:
        mexErrMsgTxt("Input S2 must be a double matrix.");
      default:
        mexErrMsgTxt("Input error.");
      }
    }
  
  /* Check to make sure the last input arguments are scalar. */
  for (int index=3; index<nrhs; index++)
    if(!mxIsDouble(prhs[index]) || 
      mxGetN(prhs[index])*mxGetM(prhs[index])!=1)
    {
      switch(index) {
      case 3:
        mexErrMsgTxt("Input M must be a scalar.");
      case 4:
        mexErrMsgTxt("Input xl must be a scalar.");
      case 5:
        mexErrMsgTxt("Input yl must be a scalar.");
      case 6:
        mexErrMsgTxt("Input zl must be a scalar.");
      case 7:
        mexErrMsgTxt("Input nu must be a scalar.");
      case 8:
        mexErrMsgTxt("Input mrx must be a scalar.");
      case 9:
        mexErrMsgTxt("Input mry must be a scalar.");
      case 10:
        mexErrMsgTxt("Input mrz must be a scalar.");
      case 11:
        mexErrMsgTxt("Input dt must be a scalar.");
      case 12:
        mexErrMsgTxt("Input Theta must be a scalar.");
      case 13:
        mexErrMsgTxt("Input CGTol must be a scalar.");
      case 14:
        mexErrMsgTxt("Input CGIter must be a scalar.");
      case 15:
        mexErrMsgTxt("Input Flag must be a scalar.");
      case 16:
        mexErrMsgTxt("Input BndRef must be a scalar.");
      default:
        mexErrMsgTxt("Input error.");
      }
    }
  
  /* Get the matrice input arguments. */
  T0 = mxGetPr(prhs[0]);
  S1 = mxGetPr(prhs[1]);
  S2 = mxGetPr(prhs[2]);
  
  /* Get the scalar input arguments. */
  M = mxGetScalar(prhs[3]);
  xl = mxGetScalar(prhs[4]);
  yl = mxGetScalar(prhs[5]);
  zl = mxGetScalar(prhs[6]);
  nu = mxGetScalar(prhs[7]);
  mrx = mxGetScalar(prhs[8]);
  mry = mxGetScalar(prhs[9]);
  mrz = mxGetScalar(prhs[10]);
  dt = mxGetScalar(prhs[11]);
  theta = mxGetScalar(prhs[12]);
  CGTol = mxGetScalar(prhs[13]);
  CGIter = mxGetScalar(prhs[14]);
  Flag = mxGetScalar(prhs[15]);
  BndRef = mxGetScalar(prhs[16]);
  
  /* Get the dimensions of the input matrix S1. */
  mrows = mxGetM(prhs[1]);
  ncols = mxGetN(prhs[1]);
  
  // test begin
  //  for (int count=0; count<mrows*ncols; count++)
  //    mexPrintf("%g ", *(T0+count));
  // test end
  
  /* Set the output pointer to the output matrix. */
  plhs[0] = mxCreateDoubleMatrix(mrows, ncols-1, mxREAL);
  
  /* Create a C pointer to a copy of the output matrix. */
  sol2D = mxGetPr(plhs[0]);
  
  /* Call the C subroutine. */
  ipdrops(sol2D, T0, S1, S2, M, xl, yl, zl, nu, mrx, mry, mrz, 
    dt, ncols-1, theta, CGTol, CGIter, Flag, BndRef);
    
  return;
  
}
