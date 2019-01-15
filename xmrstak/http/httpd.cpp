#ifndef CONF_NO_HTTPD


#include "httpd.hpp"
#include "webdesign.hpp"
#include "xmrstak/net/msgstruct.hpp"
#include "xmrstak/misc/console.hpp"
#include "xmrstak/misc/executor.hpp"
#include "xmrstak/jconf.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include <microhttpd.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#endif // _WIN32

httpd* httpd::oInst = nullptr;

httpd::httpd()
{

}

int httpd::req_handler(void * cls,
			MHD_Connection* connection,
			const char* url,
			const char* method,
			const char* version,
			const char* upload_data,
			size_t* upload_data_size,
			void ** ptr)
{
		return MHD_NO;
}

bool httpd::start_daemon()
{
	return false;
}

#endif

