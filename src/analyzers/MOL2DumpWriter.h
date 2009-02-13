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

/*! \file MOL2DumpWriter.h
	\brief Declares the MOL2DumpWriter class
*/

#include <string>

#include <boost/shared_ptr.hpp>

#include "Analyzer.h"

#ifndef __MOL2_DUMP_WRITER_H__
#define __MOL2_DUMP_WRITER_H__

//! Analyzer for writing out MOL2 dump files
/*! MOL2DumpWriter writes a single .mol2 formated file each time analyze() is called. The timestep is 
	added into the file name the same as HOOMDDumpWriter and PDBDumpWriter do.
	
	\ingroup analyzers
*/
class MOL2DumpWriter : public Analyzer
	{
	public:
		//! Construct the writer
		MOL2DumpWriter(boost::shared_ptr<ParticleData> pdata, std::string fname_base);
		
		//! Write out the data for the current timestep
		void analyze(unsigned int timestep);
		
		//! Write the mol2 file
		void writeFile(std::string fname);
	private:
		std::string m_base_fname;	//!< String used to store the file name of the output file
	};
	
//! Exports the MOL2DumpWriter class to python
void export_MOL2DumpWriter();

#endif

