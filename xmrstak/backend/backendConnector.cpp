#include "iBackend.hpp"
#include "backendConnector.hpp"
#include "miner_work.hpp"
#include "globalStates.hpp"
#include "plugin.hpp"
#include "xmrstak/misc/environment.hpp"
#include "xmrstak/misc/console.hpp"
#include "xmrstak/params.hpp"

#include "cpu/minethd.hpp"

#include <cstdlib>
#include <assert.h>
#include <cmath>
#include <chrono>
#include <cstring>
#include <thread>
#include <bitset>


namespace xmrstak
{

bool BackendConnector::self_test()
{
	return cpu::minethd::self_test();
}

std::vector<iBackend*>* BackendConnector::thread_starter(miner_work& pWork)
{

	std::vector<iBackend*>* pvThreads = new std::vector<iBackend*>;


#ifndef CONF_NO_CPU
	if(params::inst().useCPU)
	{
		auto cpuThreads = cpu::minethd::thread_starter(static_cast<uint32_t>(pvThreads->size()), pWork);
		pvThreads->insert(std::end(*pvThreads), std::begin(cpuThreads), std::end(cpuThreads));
		if(cpuThreads.size() == 0)
			printer::inst()->print_msg(L0, "WARNING: backend CPU disabled.");
	}
#endif

	globalStates::inst().iThreadCount = pvThreads->size();
	return pvThreads;
}

} // namespace xmrstak
