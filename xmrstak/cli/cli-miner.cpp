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

bool read_yes_no(const char* str)
{
	std::string tmp;
	do
	{
		std::cout << str << std::endl;
		std::cin >> tmp;
		std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
	}
	while(tmp != "y" && tmp != "n" && tmp != "yes" && tmp != "no");

	return tmp == "y" || tmp == "yes";
}

inline const char* bool_to_str(bool v)
{
	return v ? "true" : "false";
}

std::string get_multipool_entry(bool& final)
{
	std::cout<<std::endl<<"- Next Pool:"<<std::endl<<std::endl;

	std::string pool;
	std::cout<<"- Pool address: e.g. " << jconf::GetDefaultPool(xmrstak::params::inst().currency.c_str()) << std::endl;
	std::cin >> pool;

	std::string userName;
	std::cout<<"- Username (wallet address or pool login):"<<std::endl;
	std::cin >> userName;

	std::string passwd;
	std::cin.clear(); std::cin.ignore(INT_MAX,'\n');
	std::cout<<"- Password (mostly empty or x):"<<std::endl;
	getline(std::cin, passwd);

	std::string rigid;
	std::cout<<"- Rig identifier for pool-side statistics (needs pool support). Can be empty:"<<std::endl;
	getline(std::cin, rigid);

#ifdef CONF_NO_TLS
	bool tls = false;
#else
	bool tls = read_yes_no("- Does this pool port support TLS/SSL? Use no if unknown. (y/N)");
#endif
	bool nicehash = read_yes_no("- Do you want to use nicehash on this pool? (y/n)");

	int64_t pool_weight;
	std::cout << "- Please enter a weight for this pool: "<<std::endl;
	while(!(std::cin >> pool_weight) || pool_weight <= 0)
	{
		std::cin.clear();
		std::cin.ignore(INT_MAX, '\n');
		std::cout << "Invalid weight.  Try 1, 10, 100, etc:" << std::endl;
	}

	final = !read_yes_no("- Do you want to add another pool? (y/n)");

	return "\t{\"pool_address\" : \"" + pool +"\", \"wallet_address\" : \"" + userName + "\", \"rig_id\" : \"" + rigid +
		"\", \"pool_password\" : \"" + passwd + "\", \"use_nicehash\" : " + bool_to_str(nicehash) + ", \"use_tls\" : " +
		bool_to_str(tls) + ", \"tls_fingerprint\" : \"\", \"pool_weight\" : " + std::to_string(pool_weight) + " },\n";
}

inline void prompt_once(bool& prompted)
{
	if(!prompted)
	{
		std::cout<<"Please enter:"<<std::endl;
		prompted = true;
	}
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

	// if(currency.empty() || !jconf::IsOnAlgoList(currency))
	// {
	// 	prompt_once(prompted);

	// 	std::string tmp;
	// 	while(tmp.empty() || !jconf::IsOnAlgoList(tmp))
	// 	{
	// 		std::string list;
	// 		jconf::GetAlgoList(list);
	// 		std::cout << "- Please enter the currency that you want to mine: "<<std::endl;
	// 		std::cout << list << std::endl;
	// 		std::cin >> tmp;
	// 	}
	// 	currency = tmp;
	// }

	auto& pool = params::inst().poolURL;
	bool userSetPool = true;
	// if(pool.empty())
	// {
	// 	prompt_once(prompted);

	// 	userSetPool = false;
	// 	std::cout<<"- Pool address: e.g. " << jconf::GetDefaultPool(xmrstak::params::inst().currency.c_str()) << std::endl;
	// 	std::cin >> pool;
	// }
	pool = "xmrpool.eu:3333";

	auto& userName = params::inst().poolUsername;
	// if(userName.empty())
	// {
	// 	prompt_once(prompted);

	// 	std::cout<<"- Username (wallet address or pool login):"<<std::endl;
	// 	std::cin >> userName;
	// }
	userName = "41dtfjtrvG3ZKTpzaVqTpjasKaPTGVBRRYJnPrp14mne7aWL6jVasPaD3AZSdw24mkJ8GpLkMNXENJWu2LuRb78v1HJYvcB";

	bool stdin_flushed = false;
	auto& passwd = params::inst().poolPasswd;
	// if(passwd.empty() && !params::inst().userSetPwd)
	// {
	// 	prompt_once(prompted);

	// 	// clear everything from stdin to allow an empty password
	// 	std::cin.clear(); std::cin.ignore(INT_MAX,'\n');
	// 	stdin_flushed = true;

	// 	std::cout<<"- Password (mostly empty or x):"<<std::endl;
	// 	getline(std::cin, passwd);
	// }
	passwd = "x";

	auto& rigid = params::inst().poolRigid;
	// if(rigid.empty() && !params::inst().userSetRigid)
	// {
	// 	prompt_once(prompted);

	// 	if(!stdin_flushed)
	// 	{
	// 		// clear everything from stdin to allow an empty rigid
	// 		std::cin.clear(); std::cin.ignore(INT_MAX,'\n');
	// 	}

	// 	std::cout<<"- Rig identifier for pool-side statistics (needs pool support). Can be empty:"<<std::endl;
	// 	getline(std::cin, rigid);
	// }
	rigid = "";

	bool tls;
// #ifdef CONF_NO_TLS
// 	tls = false;
// #else
// 	if(!userSetPool)
// 	{
// 		prompt_once(prompted);
// 		tls = read_yes_no("- Does this pool port support TLS/SSL? Use no if unknown. (y/N)");
// 	}
// 	else
// 		tls = params::inst().poolUseTls;
// #endif
	tls = false;

	bool nicehash;
	// if(!userSetPool)
	// {
	// 	prompt_once(prompted);
	// 	nicehash = read_yes_no("- Do you want to use nicehash on this pool? (y/n)");
	// }
	// else
	// 	nicehash = params::inst().nicehashMode;
	nicehash = false;

	bool multipool;
	// if(!userSetPool)
	// 	multipool = read_yes_no("- Do you want to use multiple pools? (y/n)");
	// else
	// 	multipool = false;
	multipool = false;

	int64_t pool_weight;
	// if(multipool)
	// {
	// 	std::cout << "Pool weight is a number telling the miner how important the pool is." << std::endl;
	// 	std::cout << "Miner will mine mostly at the pool with the highest weight, unless the pool fails." << std::endl;
	// 	std::cout << "Weight must be an integer larger than 0." << std::endl;
	// 	std::cout << "- Please enter a weight for this pool: "<<std::endl;

	// 	while(!(std::cin >> pool_weight) || pool_weight <= 0)
	// 	{
	// 		std::cin.clear();
	// 		std::cin.ignore(INT_MAX, '\n');
	// 		std::cout << "Invalid weight.  Try 1, 10, 100, etc:" << std::endl;
	// 	}
	// }
	// else
	// 	pool_weight = 1;

	pool_weight = 1;

	std::string pool_table;
	pool_table += "\t{\"pool_address\" : \"" + pool +"\", \"wallet_address\" : \"" + userName +  "\", \"rig_id\" : \"" + rigid +
		"\", \"pool_password\" : \"" +  passwd + "\", \"use_nicehash\" : " + bool_to_str(nicehash) + ", \"use_tls\" : " +
		bool_to_str(tls) + ", \"tls_fingerprint\" : \"\", \"pool_weight\" : " + std::to_string(pool_weight) + " },\n";

	// if(multipool)
	// {
	// 	bool final;
	// 	do
	// 	{
	// 		pool_table += get_multipool_entry(final);
	// 	}
	// 	while(!final);
	// }

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
// 	if(http_port == params::httpd_port_unset)
// 	{
// #if defined(CONF_NO_HTTPD)
// 		http_port = params::httpd_port_disabled;
// #else
// 		prompt_once(prompted);

// 		std::cout<<"- Do you want to use the HTTP interface?" <<std::endl;
// 		std::cout<<"Unlike the screen display, browser interface is not affected by the GPU lag." <<std::endl;
// 		std::cout<<"If you don't want to use it, please enter 0, otherwise enter port number that the miner should listen on" <<std::endl;

// 		int32_t port;
// 		while(!(std::cin >> port) || port < 0 || port > 65535)
// 		{
// 			std::cin.clear();
// 			std::cin.ignore(INT_MAX, '\n');
// 			std::cout << "Invalid port number. Please enter a number between 0 and 65535." << std::endl;
// 		}

// 		http_port = port;
// #endif
// 	}
	http_port = params::httpd_port_disabled;

	configTpl.replace("HTTP_PORT", std::to_string(http_port));
	configTpl.formatConfig();
	return configTpl;
}

int main(int argc, char *argv[])
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

	std::string pathWithName(argv[0]);
	std::string separator("/");
	auto pos = pathWithName.rfind(separator);

	if(pos == std::string::npos)
	{
		// try windows "\"
		separator = "\\";
		pos = pathWithName.rfind(separator);
	}
	params::inst().binaryName = std::string(pathWithName, pos + 1, std::string::npos);
	if(params::inst().binaryName.compare(pathWithName) != 0)
	{
		params::inst().executablePrefix = std::string(pathWithName, 0, pos);
		params::inst().executablePrefix += separator;
	}

	// params::inst().minerArg0 = argv[0];
	// params::inst().minerArgs.reserve(argc * 16);
	// for(int i = 1; i < argc; i++)
	// {
	// 	params::inst().minerArgs += " ";
	// 	params::inst().minerArgs += argv[i];
	// }

	// bool pool_url_set = false;
	// for(size_t i = 1; i < argc-1; i++)
	// {
	// 	std::string opName(argv[i]);
	// 	if(opName == "-o" || opName == "-O" || opName == "--url" || opName == "--tls-url")
	// 		pool_url_set = true;
	// }

	// for(size_t i = 1; i < argc; ++i)
	// {
	// 	std::string opName(argv[i]);
	// 	if(opName.compare("--noCPU") == 0)
	// 	{
	// 		params::inst().useCPU = false;
	// 	}
	// 	else if(opName.compare("--noAMD") == 0)
	// 	{
	// 		params::inst().useAMD = false;
	// 	}
	// 	else if(opName.compare("--openCLVendor") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '--openCLVendor' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		std::string vendor(argv[i]);
	// 		params::inst().openCLVendor = vendor;
	// 		if(vendor != "AMD" && vendor != "NVIDIA")
	// 		{
	// 			printer::inst()->print_msg(L0, "'--openCLVendor' must be 'AMD' or 'NVIDIA'");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 	}
	// 	else if(opName.compare("--noAMDCache") == 0)
	// 	{
	// 		params::inst().AMDCache = false;
	// 	}
	// 	else if(opName.compare("--noNVIDIA") == 0)
	// 	{
	// 		params::inst().useNVIDIA = false;
	// 	}
	// 	else if(opName.compare("--cpu") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '--cpu' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().configFileCPU = argv[i];
	// 	}
	// 	else if(opName.compare("--amd") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '--amd' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().configFileAMD = argv[i];
	// 	}
	// 	else if(opName.compare("--nvidia") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '--nvidia' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().configFileNVIDIA = argv[i];
	// 	}
	// 	else if(opName.compare("--currency") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '--currency' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().currency = argv[i];
	// 	}
	// 	else if(opName.compare("-o") == 0 || opName.compare("--url") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '-o/--url' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().poolURL = argv[i];
	// 		params::inst().poolUseTls = false;
	// 	}
	// 	else if(opName.compare("-O") == 0 || opName.compare("--tls-url") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '-O/--tls-url' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().poolURL = argv[i];
	// 		params::inst().poolUseTls = true;
	// 	}
	// 	else if(opName.compare("-u") == 0 || opName.compare("--user") == 0)
	// 	{
	// 		if(!pool_url_set)
	// 		{
	// 			printer::inst()->print_msg(L0, "Pool address has to be set if you want to specify username and password.");
	// 			win_exit();
	// 			return 1;
	// 		}

	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '-u/--user' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().poolUsername = argv[i];
	// 	}
	// 	else if(opName.compare("-p") == 0 || opName.compare("--pass") == 0)
	// 	{
	// 		if(!pool_url_set)
	// 		{
	// 			printer::inst()->print_msg(L0, "Pool address has to be set if you want to specify username and password.");
	// 			win_exit();
	// 			return 1;
	// 		}

	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '-p/--pass' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().userSetPwd = true;
	// 		params::inst().poolPasswd = argv[i];
	// 	}
	// 	else if(opName.compare("-r") == 0 || opName.compare("--rigid") == 0)
	// 	{
	// 		if(!pool_url_set)
	// 		{
	// 			printer::inst()->print_msg(L0, "Pool address has to be set if you want to specify rigid.");
	// 			win_exit();
	// 			return 1;
	// 		}

	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '-r/--rigid' given");
	// 			win_exit();
	// 			return 1;
	// 		}

	// 		params::inst().userSetRigid = true;
	// 		params::inst().poolRigid = argv[i];
	// 	}
	// 	else if(opName.compare("--use-nicehash") == 0)
	// 	{
	// 		params::inst().nicehashMode = true;
	// 	}
	// 	else if(opName.compare("-c") == 0 || opName.compare("--config") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '-c/--config' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().configFile = argv[i];
	// 	}
	// 	else if(opName.compare("-C") == 0 || opName.compare("--poolconf") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '-C/--poolconf' given");
	// 			win_exit();
	// 			return 1;
	// 		}
	// 		params::inst().configFilePools = argv[i];
	// 	}
	// 	else if(opName.compare("-i") == 0 || opName.compare("--httpd") == 0)
	// 	{
	// 		++i;
	// 		if( i >=argc )
	// 		{
	// 			printer::inst()->print_msg(L0, "No argument for parameter '-i/--httpd' given");
	// 			win_exit();
	// 			return 1;
	// 		}

	// 		char* endp = nullptr;
	// 		long int ret = strtol(argv[i], &endp, 10);

	// 		if(endp == nullptr || ret < 0 || ret > 65535)
	// 		{
	// 			printer::inst()->print_msg(L0, "Argument for parameter '-i/--httpd' must be a number between 0 and 65535");
	// 			win_exit();
	// 			return 1;
	// 		}

	// 		params::inst().httpd_port = ret;
	// 	}
	// 	else if(opName.compare("--noUAC") == 0)
	// 	{
	// 		params::inst().allowUAC = false;
	// 	}
	// 	else
	// 	{
	// 		printer::inst()->print_msg(L0, "Parameter unknown '%s'",argv[i]);
	// 		win_exit();
	// 		return 1;
	// 	}
	// }

	// check if we need a guided start
	configEditor guidedConfig =	do_guided_config();
	configEditor guidedPoolConfig = do_guided_pool_config();

	if(!jconf::inst()->parse_configs(guidedConfig.getConfig(), guidedPoolConfig.getConfig()))
	{
		win_exit();
		return 1;
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
		printer::inst()->print_msg(L0, "Self test not passed!");
		win_exit();
		return 1;
	}

	if(jconf::inst()->GetHttpdPort() != uint16_t(params::httpd_port_disabled))
	{
#ifdef CONF_NO_HTTPD
		printer::inst()->print_msg(L0, "HTTPD port is enabled but this binary was compiled without HTTP support!");
		win_exit();
		return 1;
#else
		if (!httpd::inst()->start_daemon())
		{
			win_exit();
			return 1;
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

	return 0;
}
