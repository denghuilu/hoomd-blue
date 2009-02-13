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

/*! \file System.cc
	\brief Defines the System class
*/

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4103 4244 )
#endif

#include "System.h"
#include "SignalHandler.h"

#include <boost/python.hpp>
using namespace boost::python;

#include <stdexcept>

using namespace std;

/*! \param pdata Particle data that makes up the system to be simulated
	\param initial_tstep Time step to start counting from when run() is first called
	
	\post The System is constructed with no attached computes, updaters, 
		analyzers or integrators. Profiling defaults to disabled and
		statistics are printed every 10 seconds.
*/	
System::System(boost::shared_ptr<ParticleData> pdata, unsigned int initial_tstep)
	: m_pdata(pdata), m_start_tstep(initial_tstep), m_end_tstep(0), m_cur_tstep(initial_tstep),
		m_last_status_time(0), m_last_status_tstep(initial_tstep), m_profile(false), m_stats_period(10)
	{
	// sanity check
	assert(pdata);
	assert(pdata->getN() > 0);	
	}
	
/*! \param analyzer Shared pointer to the Analyzer to add
	\param name A unique name to identify the Analyzer by
	\param period Analyzer::analyze() will be called for every time step that is a multiple
		of \a period.
		
	All Analyzers will be called, in the order that they are added, and with the specified
	\a period during time step calculations performed when run() is called. An analyzer
	can be prevented from running in future runs by removing it (removeAnalyzer()) before 
	calling run()
*/
void System::addAnalyzer(boost::shared_ptr<Analyzer> analyzer, const std::string& name, unsigned int period)
	{
	// sanity check
	assert(analyzer);
	assert(period != 0);
	
	// first check that the name is unique
	vector<analyzer_item>::iterator i;
	for (i = m_analyzers.begin(); i != m_analyzers.end(); ++i)
		{
		if (i->m_name == name)
			{
			cerr << "***Error! Analyzer " << name << " already exists" << endl;
			throw runtime_error("System: cannot add Analyzer");
			}
		}
	
	// if we get here, we can add it
	m_analyzers.push_back(analyzer_item(analyzer, name, period, m_cur_tstep));
	}
	
/*! \param name Name of the Analyzer to find in m_analyzers
	\returns An iterator into m_analyzers of the found Analyzer
*/
std::vector<System::analyzer_item>::iterator System::findAnalyzerItem(const std::string &name)
	{
	// search for the analyzer
	vector<analyzer_item>::iterator i;
	for (i = m_analyzers.begin(); i != m_analyzers.end(); ++i)
		{
		if (i->m_name == name)
			{
			return i;
			}
		}
		
	cerr << "***Error! Analyzer " << name << " not found" << endl;
	throw runtime_error("System: cannot find Analyzer");
	// dummy return
	return m_analyzers.begin();
	}	
	
/*! \param name Name of the Analyzer to be removed
	\sa addAnalyzer()
*/
void System::removeAnalyzer(const std::string& name)
	{
	vector<analyzer_item>::iterator i = findAnalyzerItem(name);
	m_analyzers.erase(i);
	}
		
/*! \param name Name of the Analyzer to retrieve
	\returns A shared pointer to the requested Analyzer
*/
boost::shared_ptr<Analyzer> System::getAnalyzer(const std::string& name)
	{
	vector<System::analyzer_item>::iterator i = findAnalyzerItem(name);
	return i->m_analyzer;
	}	
			
/*! \param name Name of the Analyzer to modify
	\param period New period to set
*/
void System::setAnalyzerPeriod(const std::string& name, unsigned int period)
	{
	// sanity check
	assert(period != 0);
	
	vector<System::analyzer_item>::iterator i = findAnalyzerItem(name);
	i->m_period = period;
	}
	
/*! \param name Name of the Analyzer to get the period of
	\returns Period of the Analyzer
*/
unsigned int System::getAnalyzerPeriod(const std::string& name)
	{
	vector<System::analyzer_item>::iterator i = findAnalyzerItem(name);
	return i->m_period;
	}
		

// -------------- Updater get/set methods
/*! \param name Name of the Updater to find in m_updaters
	\returns An iterator into m_updaters of the found Updater
*/
std::vector<System::updater_item>::iterator System::findUpdaterItem(const std::string &name)
	{
	// search for the analyzer
	vector<System::updater_item>::iterator i;
	for (i = m_updaters.begin(); i != m_updaters.end(); ++i)
		{
		if (i->m_name == name)
			{
			return i;
			}
		}
		
	cerr << "***Error! Updater " << name << " not found" << endl;
	throw runtime_error("System: cannot find Updater");
	// dummy return
	return m_updaters.begin();
	}
		
		
/*! \param updater Shared pointer to the Updater to add
	\param name A unique name to identify the Updater by
	\param period Updater::update() will be called for every time step that is a multiple
		of \a period.
		
	All Updaters will be called, in the order that they are added, and with the specified
	\a period during time step calculations performed when run() is called. An updater
	can be prevented from running in future runs by removing it (removeUpdater()) before 
	calling run()
*/
void System::addUpdater(boost::shared_ptr<Updater> updater, const std::string& name, unsigned int period)
	{
	// sanity check
	assert(updater);
	assert(period != 0);
	
	// first check that the name is unique
	vector<updater_item>::iterator i;
	for (i = m_updaters.begin(); i != m_updaters.end(); ++i)
		{
		if (i->m_name == name)
			{
			cerr << "***Error! Updater " << name << " already exists" << endl;
			throw runtime_error("System: cannot add Updater");
			}
		}
	
	// if we get here, we can add it
	m_updaters.push_back(updater_item(updater, name, period, m_cur_tstep));
	}
		
/*! \param name Name of the Updater to be removed
	\sa addUpdater()
*/
void System::removeUpdater(const std::string& name)
	{
	vector<updater_item>::iterator i = findUpdaterItem(name);
	m_updaters.erase(i);
	}
		
/*! \param name Name of the Updater to retrieve
	\returns A shared pointer to the requested Updater
*/
boost::shared_ptr<Updater> System::getUpdater(const std::string& name)
	{
	vector<System::updater_item>::iterator i = findUpdaterItem(name);
	return i->m_updater;
	}	
			
/*! \param name Name of the Updater to modify
	\param period New period to set
*/
void System::setUpdaterPeriod(const std::string& name, unsigned int period)
	{
	// sanity check
	assert(period != 0);
	
	vector<System::updater_item>::iterator i = findUpdaterItem(name);
	i->m_period = period;
	}
	
/*! \param name Name of the Updater to get the period of
	\returns Period of the Updater
*/
unsigned int System::getUpdaterPeriod(const std::string& name)
	{
	vector<System::updater_item>::iterator i = findUpdaterItem(name);
	return i->m_period;
	}

		
// -------------- Compute get/set methods
		
/*! \param compute Shared pointer to the Compute to add
	\param name Unique name to assign to this Compute
	
	Computes are added to the System only as a convenience for naming,
	saving to restart files, and to activate profiling. They are never
	directly called by the system.
*/
void System::addCompute(boost::shared_ptr<Compute> compute, const std::string& name)
	{
	// sanity check
	assert(compute);
	
	// check if the name is unique
	map< string, boost::shared_ptr<Compute> >::iterator i = m_computes.find(name);
	if (i == m_computes.end())
		m_computes[name] = compute;
	else
		{
		cerr << "***Error! Compute " << name << " already exists" << endl;
		throw runtime_error("System: cannot add compute");
		}
	}
	
	
/*! \param name Name of the Compute to remove
*/
void System::removeCompute(const std::string& name)
	{
	// see if the compute exists to be removed
	map< string, boost::shared_ptr<Compute> >::iterator i = m_computes.find(name);
	if (i == m_computes.end())
		{
		cerr << "***Error! Compute " << name << " not found" << endl;
		throw runtime_error("System: cannot remove compute");
		}
	else
		m_computes.erase(i);
	}
		
/*! \param name Name of the compute to access
	\returns A shared pointer to the Compute as provided previosly by addCompute()
*/
boost::shared_ptr<Compute> System::getCompute(const std::string& name)
	{
	// see if the compute even exists first
	map< string, boost::shared_ptr<Compute> >::iterator i = m_computes.find(name);
	if (i == m_computes.end())
		{
		cerr << "***Error! Compute " << name << " not found" << endl;
		throw runtime_error("System: cannot retrieve compute");
		return boost::shared_ptr<Compute>();
		}
	else
		return m_computes[name];	
	}
		
// -------------- Integrator methods
		
/*! \param integrator Updater to set as the Integrator for this System
*/
void System::setIntegrator(boost::shared_ptr<Integrator> integrator)
	{
	m_integrator = integrator;
	}
		
/*! \returns A shared pointer to the Integrator for this System
*/
boost::shared_ptr<Integrator> System::getIntegrator()
	{
	return m_integrator;
	}
		
// -------------- Methods for running the simulation
		
/*! \param nsteps Number of simulation steps to run
	
	During each simulation step, all added Analyzers and
	Updaters are called, then the Integrator to move the system
	forward one step in time. This is repeated \a nsteps times.
	
	run() can be called as many times as the user wishes: 
	each time, it will continue at the time step where it left off.
*/
void System::run(unsigned int nsteps)
	{
	m_start_tstep = m_cur_tstep;
	m_end_tstep = m_cur_tstep + nsteps;
	
	// initialize the last status time
	int64_t initial_time = m_clk.getTime();
	m_last_status_time = initial_time;
	setupProfiling();
	
	if (!m_integrator)
		cout << "***Warning! You are running without an integrator." << endl;
		
	// handle time steps
	for ( ; m_cur_tstep < m_end_tstep; m_cur_tstep++)
		{
		// check the clock and output a status line if needed
		uint64_t cur_time = m_clk.getTime();
		if (cur_time - m_last_status_time >= uint64_t(m_stats_period)*uint64_t(1000000000))
			{
			generateStatusLine();
			m_last_status_time = cur_time;
			m_last_status_tstep = m_cur_tstep;
			}
		
		// execute analyzers
		vector<analyzer_item>::iterator analyzer;
		for (analyzer =  m_analyzers.begin(); analyzer != m_analyzers.end(); ++analyzer)
			{
			if (analyzer->shouldExecute(m_cur_tstep))
				analyzer->m_analyzer->analyze(m_cur_tstep);
			}
		
		// execute updaters
		vector<updater_item>::iterator updater;
		for (updater =  m_updaters.begin(); updater != m_updaters.end(); ++updater)
			{
			if (updater->shouldExecute(m_cur_tstep))
				updater->m_updater->update(m_cur_tstep);
			}
	
		// execute the integrator
		if (m_integrator)
			m_integrator->update(m_cur_tstep);
		
		// quit if cntrl-C was pressed
		if (g_sigint_recvd)
			{
			g_sigint_recvd = 0;
			return;
			}
		}
		
	// generate a final status line
	generateStatusLine();
	m_last_status_tstep = m_cur_tstep;

	// calculate averate TPS
	Scalar TPS = Scalar(m_cur_tstep - m_start_tstep) / Scalar(m_clk.getTime() - initial_time) * Scalar(1e9);
	cout << "Average TPS: " << TPS << endl;
	m_last_TPS = TPS;
		
	// write out the profile data
	if (m_profiler)
		cout << *m_profiler;

	printStats();
	}
		
/*! \param enable Set to true to enable profiling during calls to run()
*/
void System::enableProfiler(bool enable)
	{
	m_profile = enable;
	}
	
/*! \param logger Logger to register computes and updaters with
	All computes and updaters registered with the system are also registerd with the logger.
*/
void System::registerLogger(boost::shared_ptr<Logger> logger)
	{
	// set the profiler on everything
	if (m_integrator)
		logger->registerUpdater(m_integrator);
		
	// updaters
	vector<updater_item>::iterator updater;
	for (updater = m_updaters.begin(); updater != m_updaters.end(); ++updater)
		logger->registerUpdater(updater->m_updater);
		
	// computes
	map< string, boost::shared_ptr<Compute> >::iterator compute;
	for (compute = m_computes.begin(); compute != m_computes.end(); ++compute)
		logger->registerCompute(compute->second);	
	}
		
/*! \param seconds Period between statistics ouptut in seconds
*/
void System::setStatsPeriod(unsigned int seconds)
	{
	m_stats_period = seconds;
	}
		
// --------- Steps in the simulation run implemented in helper functions

void System::setupProfiling()
	{
	if (m_profile)
		m_profiler = boost::shared_ptr<Profiler>(new Profiler("Simulation"));
	else
		m_profiler = boost::shared_ptr<Profiler>();
		
	// set the profiler on everything
	if (m_integrator)
		m_integrator->setProfiler(m_profiler);
	m_pdata->setProfiler(m_profiler);
	
	// analyzers
	vector<analyzer_item>::iterator analyzer;
	for (analyzer = m_analyzers.begin(); analyzer != m_analyzers.end(); ++analyzer)
		analyzer->m_analyzer->setProfiler(m_profiler);
	
	// updaters
	vector<updater_item>::iterator updater;
	for (updater = m_updaters.begin(); updater != m_updaters.end(); ++updater)
		updater->m_updater->setProfiler(m_profiler);
		
	// computes
	map< string, boost::shared_ptr<Compute> >::iterator compute;
	for (compute = m_computes.begin(); compute != m_computes.end(); ++compute)
		compute->second->setProfiler(m_profiler);
	}

void System::printStats()
	{
	cout << "---------" << endl;
	// set the profiler on everything
	//m_integrator->printStats();
	//m_pdata->printStats();
	
	// analyzers
	//vector<analyzer_item>::iterator analyzer;
	//for (analyzer = m_analyzers.begin(); analyzer != m_analyzers.end(); ++analyzer)
	//	analyzer->m_analyzer->printStats();
	
	// updaters
	// vector<updater_item>::iterator updater;
	// for (updater = m_updaters.begin(); updater != m_updaters.end(); ++updater)
	//	updater->m_updater->printStats();
		
	// computes
	map< string, boost::shared_ptr<Compute> >::iterator compute;
	for (compute = m_computes.begin(); compute != m_computes.end(); ++compute)
		compute->second->printStats();
	}
	
void System::generateStatusLine()
	{
	// a status line consists of 
	// elapsed time
	// current timestep / end time step
	// time steps per second
	// ETA

	// elapsed time
	int64_t cur_time = m_clk.getTime();
	string t_elap = ClockSource::formatHMS(cur_time);

	// time steps per second
	Scalar TPS = Scalar(m_cur_tstep - m_last_status_tstep) / Scalar(cur_time - m_last_status_time) * Scalar(1e9);

	// estimated time to go (base on current TPS)
	string ETA = ClockSource::formatHMS(int64_t((m_end_tstep - m_cur_tstep) / TPS * Scalar(1e9)));

	// write the line
	cout << "Time " << t_elap << " | Step " << m_cur_tstep << " / " << m_end_tstep << " | TPS " << TPS << " | ETA " << ETA << endl;
	}

void export_System()
	{
	class_< System, boost::shared_ptr<System>, boost::noncopyable > ("System", init< boost::shared_ptr<ParticleData>, unsigned int >())
		.def("addAnalyzer", &System::addAnalyzer)
		.def("removeAnalyzer", &System::removeAnalyzer)
		.def("getAnalyzer", &System::getAnalyzer)
		.def("setAnalyzerPeriod", &System::setAnalyzerPeriod)
		.def("getAnalyzerPeriod", &System::getAnalyzerPeriod)

		.def("addUpdater", &System::addUpdater)
		.def("removeUpdater", &System::removeUpdater)
		.def("getUpdater", &System::getUpdater)
		.def("setUpdaterPeriod", &System::setUpdaterPeriod)
		.def("getUpdaterPeriod", &System::getUpdaterPeriod)

		.def("addCompute", &System::addCompute)
		.def("removeCompute", &System::removeCompute)
		.def("getCompute", &System::getCompute)

		.def("setIntegrator", &System::setIntegrator)
		.def("getIntegrator", &System::getIntegrator)

		.def("registerLogger", &System::registerLogger)
		.def("setStatsPeriod", &System::setStatsPeriod)
		.def("enableProfiler", &System::enableProfiler)
		.def("run", &System::run)
		
		.def("getLastTPS", &System::getLastTPS)
		.def("getCurrentTimeStep", &System::getCurrentTimeStep)
		;
	}

#ifdef WIN32
#pragma warning( pop )
#endif
