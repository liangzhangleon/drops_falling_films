#include "geom/multigrid.h"
#include "geom/builder.h"
#include "poisson/poisson.h"
#include "out/output.h"
#include "num/solver.h"
#include <fstream>

// laplace u + q*u = f
class PoissonCoeffCL
{
  public:
    static const double a, b;
    static double q(const DROPS::Point3DCL&) { return 0.0; }
    static double f(const DROPS::Point3DCL& p)
    {
        const double t0= p.norm();
        const double t1= exp(a*(t0-b));
        if (t0<1.e-5 || t1 > 1.e10)
            return 0.;
        else
        {
            const double t2= 1.0+t1;
            return a*t1*(-2.0*t2 -a*t0+a*t1*t0)/(std::pow(t2,3)*t0);
        }
    }

};

const double PoissonCoeffCL::a= -200.;
const double PoissonCoeffCL::b= .8;

inline double Lsg( const DROPS::Point3DCL& p)
{
    return 1/(1.0+exp(PoissonCoeffCL::a*(p.norm()-PoissonCoeffCL::b)));
}


namespace DROPS // for Strategy
{

template<class MGB, class Coeff>
void Strategy(PoissonP1CL<MGB,Coeff>& Poisson, double omega, double rel_red)
{
    typedef PoissonP1CL<MGB,Coeff> MyPoissonCL;

    MultiGridCL& MG= Poisson.GetMG();
    const typename MyPoissonCL::BndDataCL& BndData= Poisson.GetBndData();
    
    IdxDescCL  loc_idx;
    VecDescCL  loc_x;
    
    IdxDescCL* new_idx= &Poisson.idx;
    IdxDescCL* old_idx= &loc_idx;
//    IdxDescCL* err_idx= &_err_idx;
    VecDescCL* new_x= &Poisson.x;
    VecDescCL* old_x= &loc_x;
    VecDescCL* b= &Poisson.b;
//    VecDescCL* err= &_err;
    MatDescCL* A= &Poisson.A;
    SSORPcCL   pc(omega);
    DoerflerMarkCL<typename MyPoissonCL::est_fun, typename MyPoissonCL::_base>
        Estimator(rel_red, 0.0, 0.6, 8, true, &MyPoissonCL::ResidualErrEstimator, *static_cast<typename MyPoissonCL::_base*>(&Poisson) );
    Uint step= 0;
    bool new_marks;

    new_idx->Set( 0, 1, 0, 0, 0);
    old_idx->Set( 1, 1, 0, 0, 0);
//    err_idx->Set( 2, 0, 0, 0, 1);
    do
    {
        std::cerr << DROPS::SanityMGOutCL(MG) << std::endl;
        MG.Refine();
        Poisson.CreateNumbering(MG.GetLastLevel(), new_idx);    // erzeuge Nummerierung zu diesem Index
        std::cerr << "altes und neues TriangLevel: " << old_idx->TriangLevel << ", "
                  << new_idx->TriangLevel << std::endl;
        b->SetIdx( new_idx);                        // Erster Vektor aus new_idx
        new_x->SetIdx( new_idx);                    // Zweiter Vektor ebenfalls aus new_idx
        std::cerr << "Anzahl der Unbekannten: " << old_x->Data.size() << ", "
                  << new_x->Data.size() << std::endl;
        MG.SizeInfo(std::cerr);
        if (step==0)
        {
            Estimator.Init(typename MyPoissonCL::DiscSolCL(new_x, &BndData, &MG) );
        }
        if (old_x->RowIdx)
        {
            P1EvalCL<double, const PoissonBndDataCL, const VecDescCL>  oldx(old_x, &BndData, &MG);
            P1EvalCL<double, const PoissonBndDataCL, VecDescCL>        newx(new_x, &BndData, &MG);
// TODO: what happens, if refinement creates new vertices in old_level? FIXED in fe.h!       
            Interpolate(newx, oldx);
//            CheckSolution(*new_x, &::Lsg);
            old_x->Reset();
        }
        A->SetIdx(new_idx, new_idx);               // Erste Matrix aus Index 0 (Zeilen und Spalten)
        Poisson.SetupSystem(*A, *b);
//        Poisson.GetDiscError(&::Lsg);
//std::cout << A->Data << std::endl << b->Data << std::endl;
        double tol= 1.0e-7;
        int max_iter= 100;        
        PCG(A->Data, new_x->Data, b->Data, pc, max_iter, tol);
//        CG(A->Data, new_x->Data, b->Data, max_iter, tol);
        A->Reset();
        b->Reset();
        std::cerr << "Iterationen: " << max_iter << "    Norm des Residuums: " << tol << std::endl;
//        std::cerr << "Loesung: " << new_x->Data << std::endl;
        Poisson.CheckSolution(*new_x, &::Lsg);
//        CreateNumbering(new_idx->TriangLevel, err_idx);
//        err->SetIdx( err_idx);
//        NumMarked= EstimateError(*new_x, 0.2, &Estimator);
        new_marks= Estimator.Estimate( typename MyPoissonCL::DiscSolCL(new_x, &BndData, &MG) );
//        err->Reset();
//        DeleteNumbering(err_idx);
        std::swap(old_x, new_x);
        std::swap(old_idx, new_idx);
        std::cerr << std::endl;
    }
    while (new_marks && step++<9);
    // I want the solution to be in Poisson.x
    if (old_x == &loc_x)
    {
        Poisson.idx= loc_idx;
        Poisson.x.SetIdx(&Poisson.idx);

        Poisson.x.Data.resize(loc_x.Data.size());
        Poisson.x.Data= loc_x.Data;
    }
}

} // end of namespace DROPS

void MarkDrop (DROPS::MultiGridCL& mg, DROPS::Uint maxLevel)
{
    DROPS::Point3DCL Mitte; Mitte[0]=0.5; Mitte[1]=0.5; Mitte[2]=0.5;

    for (DROPS::MultiGridCL::TriangTetraIteratorCL It(mg.GetTriangTetraBegin(maxLevel)),
             ItEnd(mg.GetTriangTetraEnd(maxLevel)); It!=ItEnd; ++It)
    {
        if ( (GetBaryCenter(*It)-Mitte).norm()<=std::max(0.1,1.5*std::pow(It->GetVolume(),1.0/3.0)) )
            It->SetRegRefMark();
    }
}


void UnMarkDrop (DROPS::MultiGridCL& mg, DROPS::Uint maxLevel)
{
    DROPS::Point3DCL Mitte; Mitte[0]=0.5; Mitte[1]=0.5; Mitte[2]=0.5;

    for (DROPS::MultiGridCL::TriangTetraIteratorCL It(mg.GetTriangTetraBegin(maxLevel)),
             ItEnd(mg.GetTriangTetraEnd(maxLevel)); It!=ItEnd; ++It)
    {
        if ( (GetBaryCenter(*It)-Mitte).norm()<=std::max(0.1,1.5*std::pow(It->GetVolume(),1.0/3.0)) )
            It->SetRemoveMark();
    }
}

// boundary functions (neumann, dirichlet type)
// used for BndSegCL-object of a UnitCube
inline double neu_val(const DROPS::Point2DCL& p) { throw "Don't use Neumann-values."; }
inline double dir_val(const DROPS::Point2DCL&) { return 1.0; }


int main (int argc, char** argv)
{
  try
  {
    if (argc!=3)
    {
        std::cerr << "You have to specify two parameters:\n\tdrops <omega> <err_red>" << std::endl;
        return 1;
    }
    double omega;
    double rel_red;
    DROPS::Point3DCL orig(-1.0);
    DROPS::Point3DCL e1(0.0), e2(0.0), e3(0.0);
    e1[0]= e2[1]= e3[2]= 2.0;

    typedef DROPS::PoissonP1CL<DROPS::BrickBuilderCL, PoissonCoeffCL> PoissonOnBCL;
    typedef PoissonOnBCL                                              MyPoissonCL;
    
    DROPS::BrickBuilderCL brick(orig, e1, e2, e3, 4, 4, 4);
    const bool isneumann[6]= 
        {false, false, false, false, false, false};
    const DROPS::PoissonBndDataCL::bnd_val_fun bnd_fun[6]=
        { &dir_val, &dir_val, &dir_val, &dir_val, &dir_val, &dir_val};
 
    DROPS::PoissonBndDataCL bdata(6, isneumann, bnd_fun);
    PoissonOnBCL prob(brick, PoissonCoeffCL(), bdata);
    DROPS::MultiGridCL& mg = prob.GetMG();
    DROPS::RBColorMapperCL colormap;

    omega= atof(argv[1]);
    rel_red= atof(argv[2]);
    std::cerr << "Omega: " << omega << " rel_red: " << rel_red << std::endl;
    DROPS::Strategy(prob, omega, rel_red);
    std::cerr << "hallo" << std::endl;
    std::cerr << DROPS::SanityMGOutCL(mg) << std::endl;
    std::ofstream fil("ttt.off");
    fil << DROPS::GeomSolOutCL<MyPoissonCL::DiscSolCL>(mg, prob.GetSolution(), &colormap, -1, false, 0.0, prob.x.Data.min(), prob.x.Data.max()) << std::endl;
    return 0;
  }
  catch (DROPS::DROPSErrCL err) { err.handle(); }
}
