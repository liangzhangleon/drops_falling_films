/// \file
/// \brief classes that constitute the 2-phase Navier-Stokes problem

#ifndef DROPS_INSTATNAVSTOKES2PHASE_H
#define DROPS_INSTATNAVSTOKES2PHASE_H

#include "stokes/instatstokes2phase.h"


namespace DROPS
{

/// problem class for instationary two-pase Navier-Stokes flow

template <class Coeff>
class InstatNavierStokes2PhaseP2P1CL : public InstatStokes2PhaseP2P1CL<Coeff>
{
  private:
    typedef InstatStokes2PhaseP2P1CL<Coeff>       _base;
    typedef InstatNavierStokes2PhaseP2P1CL<Coeff> _self;

  public:
    using _base::GetBndData;
    using _base::GetMG;
    using _base::_Coeff;
    using _base::_MG;
    using _base::_BndData;
    using _base::b;
    using _base::c;
    using _base::A;
    using _base::B;
    
    typedef Coeff                              CoeffCL;
    typedef typename _base::BndDataCL          BndDataCL;
    typedef typename _base::DiscVelSolCL       DiscVelSolCL;
    typedef typename _base::const_DiscVelSolCL const_DiscVelSolCL;
  
    MatDescCL    N;
    
    InstatNavierStokes2PhaseP2P1CL(const MGBuilderCL& mgb, const CoeffCL& coeff, const BndDataCL& bdata)
        : InstatStokes2PhaseP2P1CL<Coeff>( mgb, coeff, bdata) {}  
    InstatNavierStokes2PhaseP2P1CL(MultiGridCL& mg, const CoeffCL& coeff, const BndDataCL& bdata)
        : InstatStokes2PhaseP2P1CL<Coeff>( mg, coeff, bdata) {}  

    /// \name Discretization
    //@{
    /// \brief Set up matrix for nonlinearity 
    void SetupNonlinear(MatDescCL* matN, const VelVecDescCL* vel, VelVecDescCL* cplN, const LevelsetP2CL& lset, double t) const;
    //@}
};

} // end of namespace DROPS

#include "navstokes/instatnavstokes2phase.tpp"

#endif
