/// \file bndmap.cpp
/// \brief global map for boundary functions
/// \author LNM RWTH Aachen: Martin Horsky; SC RWTH Aachen:
/*
 * This file is part of DROPS.
 *
 * DROPS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DROPS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DROPS. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright 2009 LNM/SC RWTH Aachen, Germany
*/

#include<map>
#include "misc/bndmap.h"

namespace DROPS
{

    template<class T>
    SingletonBaseCL<T>& SingletonBaseCL<T>::getInstance()
    {
       static SingletonBaseCL instance;
       return instance;
    }

    template<class T>
    T SingletonBaseCL<T>::operator[](std::string s){
        if (this->find(s) == this->end()){
            std::ostringstream os;
            os << "function with the name \"" << s << "\" not found in container!";
            throw DROPSErrCL(os.str());
        }
        return this->find(s)->second;
    }

    RegisterVelFunction::RegisterVelFunction(std::string name, instat_vector_fun_ptr fptr){
        InVecMap::getInstance().insert(std::make_pair(name,fptr));
    }

    RegisterScalarFunction::RegisterScalarFunction(std::string name, instat_scalar_fun_ptr fptr){
        InScaMap::getInstance().insert(std::make_pair(name,fptr));
    }
    
    RegisterScalarFunction::RegisterScalarFunction(std::string name, scalar_fun_ptr fptr){
        ScaMap::getInstance().insert(std::make_pair(name,fptr));
    }



    template class SingletonBaseCL<DROPS::instat_scalar_fun_ptr>;
    template class SingletonBaseCL<DROPS::instat_vector_fun_ptr>;
    template class SingletonBaseCL<DROPS::scalar_fun_ptr>;

} //end of namespace DROPS
