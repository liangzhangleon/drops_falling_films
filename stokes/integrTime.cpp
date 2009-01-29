//**************************************************************************
// File:    integrTime.cpp                                                 *
// Content: Stokes-preconditioner an time integration                      *
// Author:  Sven Gross, Joerg Grande, Hieu Nguyen, IGPM RWTH Aachen        *
//          Oliver Fortmeiere, SC RWTH Aachen                              *
// Version: 0.1                                                            *
// History: begin - Jan, 24 2008                                           *
//**************************************************************************

#include <stokes/integrTime.h>

namespace DROPS
{

void ISBBTPreCL::Update() const
{
    IF_MASTER
      std::cerr << "ISBBTPreCL::Update: old version: " << Bversion_
                << "\tnew version: " << B_->Version() << '\n';
    delete Bs_;
    Bs_= new MatrixCL( *B_);
    Bversion_= B_->Version();

    BBT_.SetBlock0( Bs_);
    BBT_.SetBlock1( Bs_);

#ifndef _PAR
    VectorCL Dvelinv( 1.0/ Mvel_->GetDiag());
#else
    VectorCL Dvelinv( 1.0/ vel_idx_.GetEx().GetAccumulate(Mvel_->GetDiag()));
#endif
    ScaleCols( *Bs_, VectorCL( std::sqrt( Dvelinv)));

#ifndef _PAR
    VectorCL Dprsqrt( std::sqrt( M_->GetDiag()));
#else
    VectorCL Dprsqrt( std::sqrt( pr_idx_.GetEx().GetAccumulate( M_->GetDiag())));
#endif
    Dprsqrtinv_.resize( M_->num_rows());
    Dprsqrtinv_= 1.0/Dprsqrt;

    ScaleRows( *Bs_, Dprsqrtinv_);

#ifndef _PAR    // Skipp computing diag of BB^T in parallel ...
    D_.resize( M_->num_rows());
    D_= 1.0/BBTDiag( *Bs_);
#endif
}

#ifndef _PAR
void MinCommPreCL::Update() const
{
    std::cerr << "MinCommPreCL::Update: old/new versions: " << Aversion_  << '/' << A_->Version()
        << '\t' << Bversion_ << '/' << B_->Version() << '\t' << Mversion_ << '/' << M_->Version()
        << '\t' << Mvelversion_ << '/' << Mvel_->Version() << '\n';
    delete Bs_;
    Bs_= new MatrixCL( *B_);
    Aversion_= A_->Version();
    Bversion_= B_->Version();
    Mversion_= M_->Version();
    Mvelversion_= Mvel_->Version();

    BBT_.SetBlock0( Bs_);
    BBT_.SetBlock1( Bs_);

    Assert( Mvel_->GetDiag().min() > 0., "MinCommPreCL::Update: Mvel_->GetDiag().min() <= 0\n", DebugNumericC);
    VectorCL Dvelsqrt( std::sqrt( Mvel_->GetDiag()));
    Dvelsqrtinv_.resize( Mvel_->num_rows());
    Dvelsqrtinv_= 1.0/Dvelsqrt;
    ScaleCols( *Bs_, Dvelsqrtinv_);

    Assert( M_->GetDiag().min() > 0., "MinCommPreCL::Update: M_->GetDiag().min() <= 0\n", DebugNumericC);
    VectorCL Dprsqrt( std::sqrt( M_->GetDiag()));
    Dprsqrtinv_.resize( M_->num_rows());
    Dprsqrtinv_= 1.0/Dprsqrt;
    ScaleRows( *Bs_, Dprsqrtinv_);

    D_.resize( M_->num_rows());
    D_= 1.0/BBTDiag( *Bs_);
}
#endif
}
