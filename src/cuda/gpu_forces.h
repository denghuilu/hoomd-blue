/*
Highly Optimized Object-Oriented Molecular Dynamics (HOOMD) Open
Source Software License
Copyright (c) 2008 Ames Laboratory Iowa State University
All rights reserved.

Redistribution and use of HOOMD, in source and binary forms, with or
without modification, are permitted, provided that the following
conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names HOOMD's
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND
CONTRIBUTORS ``AS IS''  AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*/

// $Id$
// $URL$

#ifndef _CUDA_FORCES_H_
#define _CUDA_FORCES_H_

#include <stdio.h>
#include <cuda_runtime.h>

#include "gpu_pdata.h"
#include "gpu_nlist.h"

/*! \file gpu_forces.h
 	\brief Declares functions and data structures for calculating forces on the GPU
 	\details Functions in this file are NOT to be called by anyone not knowing 
		exactly what they are doing. They are designed to be used solely by 
		LJForceComputeGPU and BondForceComputeGPU.
*/

extern "C" {

//////////////////////////////////////////////////////////////////////////
// bond force data structures

//! Bond data stored on the GPU
/*! gpu_bondtable_array stores all of the bonds between particles on the GPU.
	It is structured similar to gpu_nlist_array in that a single column in the list
	stores all of the bonds for the particle associated with that column. 
	
	To access bond \em b of particle \em i, use the following indexing scheme
	\code
	uint2 bond = bondtable.bonds[b*bondtable.pitch + i]
	\endcode
	The particle with \b index (not tag) \em i is bonded to particle \em bond.x
	with bond type \em bond.y. Each particle may have a different number of bonds as
	indicated in \em n_bonds[i].
*/
struct gpu_bondtable_array
	{
	unsigned int *n_bonds;	//!< Number of bonds for each particle
	uint2 *bonds;			//!< bond list
	unsigned int height;	//!< height of the bond list
	unsigned int pitch;		//!< width (in elements) of the bond list
	int *checkr;            //! used to see if bond length condition is violated (fene)
	};


///////////////////////////// LJ params

//! Perform the lj force calculation
cudaError_t gpu_ljforce_sum(float4 *d_forces, gpu_pdata_arrays *pdata, gpu_boxsize *box, gpu_nlist_array *nlist, float2 *d_coeffs, int coeff_width, float r_cutsq, int M);

//////////////////////////// Stochastic Bath

//! Add a Stochastic Bath for BD NVT
cudaError_t gpu_stochasticforce(float4 *d_forces, gpu_pdata_arrays *pdata, float2 d_dt_T, float1 *d_gammas, uint4 *d_state, int gamma_length, int M);


//////////////////////////// Bond table stuff

//! Sum bond forces
cudaError_t gpu_bondforce_sum(float4 *d_forces, gpu_pdata_arrays *pdata, gpu_boxsize *box, gpu_bondtable_array *btable, float2 *d_params, unsigned int n_bond_types, int block_size);


//! Sum fene bond forces
cudaError_t gpu_fenebondforce_sum(float4 *d_forces, gpu_pdata_arrays *pdata, gpu_boxsize *box, gpu_bondtable_array *btable, float4 *d_params, unsigned int n_bond_types, int block_size, unsigned int& exceedsR0);
}

#endif
