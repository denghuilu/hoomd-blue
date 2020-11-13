// Copyright (c) 2009-2019 The Regents of the University of Michigan
// This file is part of the HOOMD-blue project, released under the BSD 3-Clause License.


// Maintainer: joaander

#include "TwoStepRATTLEBD.h"
#include "EvaluatorConstraintManifold.h"

#pragma once

#include <pybind11/pybind11.h>

#ifdef __HIPCC__
#error This header cannot be compiled by nvcc
#endif

//! Implements Brownian dynamics on the GPU
/*! GPU accelerated version of TwoStepBD

    \ingroup updaters
*/
class PYBIND11_EXPORT TwoStepRATTLEBDGPU : public TwoStepRATTLEBD
    {
    public:
        //! Constructs the integration method and associates it with the system
        TwoStepRATTLEBDGPU(std::shared_ptr<SystemDefinition> sysdef,
                     std::shared_ptr<ParticleGroup> group,
                     std::shared_ptr<Manifold> manifold,
                     std::shared_ptr<Variant> T,
                     unsigned int seed,
                     Scalar eta = 0.000001);

        virtual ~TwoStepRATTLEBDGPU() {};

        //! Performs the first step of the integration
        virtual void integrateStepOne(unsigned int timestep);

        //! Performs the second step of the integration
        virtual void integrateStepTwo(unsigned int timestep);

        //! Includes the RATTLE forces to the virial/net force
        virtual void IncludeRATTLEForce(unsigned int timestep);

    protected:
        unsigned int m_block_size;               //!< block size
	    EvaluatorConstraintManifold m_manifoldGPU;
        GPUArray<unsigned int>  m_groupTags; //! Stores list converting group index to global tag
    };

//! Exports the TwoStepRATTLEBDGPU class to python
void export_TwoStepRATTLEBDGPU(pybind11::module& m);