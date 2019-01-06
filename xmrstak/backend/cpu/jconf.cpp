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

#include "jconf.hpp"
#include "xmrstak/misc/console.hpp"
#include "xmrstak/misc/jext.hpp"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>

#ifdef _WIN32
#define strcasecmp _stricmp
#include <intrin.h>
#else
#include <cpuid.h>
#endif


namespace xmrstak
{
namespace cpu
{

using namespace rapidjson;

/*
 * This enum needs to match index in oConfigValues, otherwise we will get a runtime error
 */
enum configEnum { aCpuThreadsConf, sUseSlowMem };

struct configVal {
	configEnum iName;
	const char* sName;
	Type iType;
};

// Same order as in configEnum, as per comment above
// kNullType means any type
configVal oConfigValues[] = {
	{ aCpuThreadsConf, "cpu_threads_conf", kNullType }
};

constexpr size_t iConfigCnt = (sizeof(oConfigValues)/sizeof(oConfigValues[0]));

inline bool checkType(Type have, Type want)
{
	if(want == have)
		return true;
	else if(want == kNullType)
		return true;
	else if(want == kTrueType && have == kFalseType)
		return true;
	else if(want == kFalseType && have == kTrueType)
		return true;
	else
		return false;
}

struct jconf::opaque_private
{
	Document jsonDoc;
	const Value* configValues[iConfigCnt]; //Compile time constant

	opaque_private()
	{
	}
};

jconf* jconf::oInst = nullptr;

jconf::jconf()
{
	prv = new opaque_private();
}

bool jconf::GetThreadConfig(size_t id, thd_cfg &cfg)
{
	if(!prv->configValues[aCpuThreadsConf]->IsArray())
		return false;

	if(id >= prv->configValues[aCpuThreadsConf]->Size())
		return false;

	const Value& oThdConf = prv->configValues[aCpuThreadsConf]->GetArray()[id];

	if(!oThdConf.IsObject())
		return false;

	const Value *mode, *no_prefetch, *aff, *asm_version;
	mode = GetObjectMember(oThdConf, "low_power_mode");
	no_prefetch = GetObjectMember(oThdConf, "no_prefetch");
	aff = GetObjectMember(oThdConf, "affine_to_cpu");
	asm_version = GetObjectMember(oThdConf, "asm");

	if(mode == nullptr || no_prefetch == nullptr || aff == nullptr || asm_version == nullptr)
		return false;

	if(!mode->IsBool() && !mode->IsNumber())
		return false;

	if(!no_prefetch->IsBool())
		return false;

	if(!aff->IsNumber() && !aff->IsBool())
		return false;

	if(aff->IsNumber() && aff->GetInt64() < 0)
		return false;

	if(mode->IsNumber())
		cfg.iMultiway = (int)mode->GetInt64();
	else
		cfg.iMultiway = mode->GetBool() ? 2 : 1;

	cfg.bNoPrefetch = no_prefetch->GetBool();

	if(aff->IsNumber())
		cfg.iCpuAff = aff->GetInt64();
	else
		cfg.iCpuAff = -1;

	if(!asm_version->IsString())
		return false;
	cfg.asm_version_str = asm_version->GetString();

	return true;
}


size_t jconf::GetThreadCount()
{
	if(prv->configValues[aCpuThreadsConf]->IsArray())
		return prv->configValues[aCpuThreadsConf]->Size();
	else
		return 0;
}

bool jconf::parse_config(const std::string& confName, const std::string& sConfig)
{
	size_t flen = sConfig.size();

	if(flen >= 64*1024)
	{
		printer::inst()->print_msg(L0, "Oversized config - %s.", confName.c_str());
		return false;
	}

	if(flen <= 16)
	{
		printer::inst()->print_msg(L0, "conifg is empty or too short - %s.", confName.c_str());
		return false;
	}

	std::vector<char> buffer(flen + 3);
	std::copy(sConfig.begin(), sConfig.end(), buffer.begin());

	//Replace Unicode BOM with spaces - we always use UTF-8
	if(static_cast<unsigned char>(buffer[1]) == 0xEF && static_cast<unsigned char>(buffer[2]) == 0xBB && static_cast<unsigned char>(buffer[3]) == 0xBF)
	{
		buffer[1] = ' ';
		buffer[2] = ' ';
		buffer[3] = ' ';
	}

	buffer[0] = '{';
	buffer[flen] = '}';
	buffer[flen + 1] = '\0';

	printer::inst()->print_msg(L0, buffer.data());

	prv->jsonDoc.Parse<kParseCommentsFlag|kParseTrailingCommasFlag>(buffer.data(), flen+2);

	if(prv->jsonDoc.HasParseError())
	{
		printer::inst()->print_msg(L0, "JSON config parse error in '%s' (offset %llu): %s",
			confName.c_str(), int_port(prv->jsonDoc.GetErrorOffset()), GetParseError_En(prv->jsonDoc.GetParseError()));
		return false;
	}

	if(!prv->jsonDoc.IsObject())
	{ //This should never happen as we created the root ourselves
		printer::inst()->print_msg(L0, "Invalid config '%s'. No root?", confName.c_str());
		return false;
	}

	for(size_t i = 0; i < iConfigCnt; i++)
	{
		if(oConfigValues[i].iName != i)
		{
			printer::inst()->print_msg(L0, "Code error. oConfigValues are not in order.");
			return false;
		}

		prv->configValues[i] = GetObjectMember(prv->jsonDoc, oConfigValues[i].sName);

		if(prv->configValues[i] == nullptr)
		{
			printer::inst()->print_msg(L0, "Invalid config '%s'. Missing value \"%s\".", confName.c_str(), oConfigValues[i].sName);
			return false;
		}

		if(!checkType(prv->configValues[i]->GetType(), oConfigValues[i].iType))
		{
			printer::inst()->print_msg(L0, "Invalid config '%s'. Value \"%s\" has unexpected type.", confName.c_str(), oConfigValues[i].sName);
			return false;
		}
	}

	thd_cfg c;
	for(size_t i=0; i < GetThreadCount(); i++)
	{
		if(!GetThreadConfig(i, c))
		{
			printer::inst()->print_msg(L0, "Thread %llu has invalid config.", int_port(i));
			return false;
		}
	}

	return true;
}

} // namespace cpu
} // namespace xmrstak
