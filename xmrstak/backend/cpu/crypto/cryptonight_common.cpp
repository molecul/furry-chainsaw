extern "C"
{
#include "c_groestl.h"
#include "c_blake256.h"
#include "c_jh.h"
#include "c_skein.h"
}
#include "xmrstak/backend/cryptonight.hpp"
#include "cryptonight.h"
#include "cryptonight_aesni.h"
#include "xmrstak/misc/console.hpp"
#include "xmrstak/jconf.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#ifdef __GNUC__
#include <mm_malloc.h>
#else
#include <malloc.h>
#endif // __GNUC__

#if defined(__APPLE__)
#include <mach/vm_statistics.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <ntsecapi.h>
#else
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#endif // _WIN32

void do_blake_hash(const void* input, uint32_t len, char* output) {
	blake256_hash((uint8_t*)output, (const uint8_t*)input, len);
}

void do_groestl_hash(const void* input, uint32_t len, char* output) {
	groestl((const uint8_t*)input, len * 8, (uint8_t*)output);
}

void do_jh_hash(const void* input, uint32_t len, char* output) {
	jh_hash(32 * 8, (const uint8_t*)input, 8 * len, (uint8_t*)output);
}

void do_skein_hash(const void* input, uint32_t len, char* output) {
	skein_hash(8 * 32, (const uint8_t*)input, 8 * len, (uint8_t*)output);
}

void (* const extra_hashes[4])(const void *, uint32_t, char *) = {do_blake_hash, do_groestl_hash, do_jh_hash, do_skein_hash};

#ifdef _WIN32
#include "xmrstak/misc/uac.hpp"

BOOL bRebootDesirable = FALSE; //If VirtualAlloc fails, suggest a reboot

BOOL AddPrivilege(TCHAR* pszPrivilege)
{
	HANDLE           hToken;
	TOKEN_PRIVILEGES tp;
	BOOL             status;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return FALSE;

	if (!LookupPrivilegeValue(NULL, pszPrivilege, &tp.Privileges[0].Luid))
		return FALSE;

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	status = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

	if (!status || (GetLastError() != ERROR_SUCCESS))
		return FALSE;

	CloseHandle(hToken);
	return TRUE;
}

BOOL AddLargePageRights()
{
	HANDLE hToken;
	PTOKEN_USER user = NULL;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) == TRUE)
	{
		TOKEN_ELEVATION Elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);
		BOOL bIsElevated = FALSE;

		if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize))
			bIsElevated = Elevation.TokenIsElevated;

		DWORD size = 0;
		GetTokenInformation(hToken, TokenUser, NULL, 0, &size);

		if (size > 0 && bIsElevated)
		{
			user = (PTOKEN_USER)LocalAlloc(LPTR, size);
			GetTokenInformation(hToken, TokenUser, user, size, &size);
		}

		CloseHandle(hToken);
	}

	if (!user)
		return FALSE;

	LSA_HANDLE handle;
	LSA_OBJECT_ATTRIBUTES attributes;
	ZeroMemory(&attributes, sizeof(attributes));

	BOOL result = FALSE;
	if (LsaOpenPolicy(NULL, &attributes, POLICY_ALL_ACCESS, &handle) == 0)
	{
		LSA_UNICODE_STRING lockmem;
		lockmem.Buffer = L"SeLockMemoryPrivilege";
		lockmem.Length = 42;
		lockmem.MaximumLength = 44;

		PLSA_UNICODE_STRING rights = NULL;
		ULONG cnt = 0;
		BOOL bHasRights = FALSE;
		if (LsaEnumerateAccountRights(handle, user->User.Sid, &rights, &cnt) == 0)
		{
			for (size_t i = 0; i < cnt; i++)
			{
				if (rights[i].Length == lockmem.Length &&
					memcmp(rights[i].Buffer, lockmem.Buffer, 42) == 0)
				{
					bHasRights = TRUE;
					break;
				}
			}

			LsaFreeMemory(rights);
		}

		if(!bHasRights)
			result = LsaAddAccountRights(handle, user->User.Sid, &lockmem, 1) == 0;

		LsaClose(handle);
	}

	LocalFree(user);
	return result;
}
#endif

size_t cryptonight_init(size_t use_fast_mem, size_t use_mlock, alloc_msg* msg)
{
#ifdef _WIN32
	if(use_fast_mem == 0)
		return 1;

	if(AddPrivilege(TEXT("SeLockMemoryPrivilege")) == 0)
	{
		RequestElevation();

		if(AddLargePageRights())
		{
			msg->warning = "";
			bRebootDesirable = TRUE;
		}
		else
			msg->warning = "";

		return 0;
	}

	bRebootDesirable = TRUE;
	return 1;
#else
	return 1;
#endif // _WIN32
}

cryptonight_ctx* cryptonight_alloc_ctx(size_t use_fast_mem, size_t use_mlock, alloc_msg* msg)
{
	size_t hashMemSize = std::max(
		cn_select_memory(::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgo()),
		cn_select_memory(::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgoRoot())
	);

	cryptonight_ctx* ptr = (cryptonight_ctx*)_mm_malloc(sizeof(cryptonight_ctx), 4096);

	if(use_fast_mem == 0)
	{
		// use 2MiB aligned memory
		ptr->long_state = (uint8_t*)_mm_malloc(hashMemSize, hashMemSize);
		ptr->ctx_info[0] = 0;
		ptr->ctx_info[1] = 0;
		return ptr;
	}

#ifdef _WIN32
	SIZE_T iLargePageMin = GetLargePageMinimum();

	if(hashMemSize > iLargePageMin)
		iLargePageMin *= 2;

	ptr->long_state = (uint8_t*)VirtualAlloc(NULL, iLargePageMin,
		MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);

	if(ptr->long_state == NULL)
	{
		_mm_free(ptr);
		if(bRebootDesirable)
			msg->warning = "";
		else
			msg->warning = "";
		return NULL;
	}
	else
	{
		ptr->ctx_info[0] = 1;
		return ptr;
	}
#else
//http://man7.org/linux/man-pages/man2/mmap.2.html
#if defined(__APPLE__)
	ptr->long_state  = (uint8_t*)mmap(NULL, hashMemSize, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON, VM_FLAGS_SUPERPAGE_SIZE_2MB, 0);
#elif defined(__FreeBSD__)
	ptr->long_state = (uint8_t*)mmap(NULL, hashMemSize, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_ALIGNED_SUPER | MAP_PREFAULT_READ, -1, 0);
#elif defined(__OpenBSD__)
	ptr->long_state = (uint8_t*)mmap(NULL, hashMemSize, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON, -1, 0);
#else
	ptr->long_state = (uint8_t*)mmap(NULL, hashMemSize, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE, -1, 0);
#endif

	if (ptr->long_state == MAP_FAILED)
	{
		_mm_free(ptr);
		msg->warning = "";
		return NULL;
	}

	ptr->ctx_info[0] = 1;

	if(madvise(ptr->long_state, hashMemSize, MADV_RANDOM|MADV_WILLNEED) != 0)
		msg->warning = "";

	ptr->ctx_info[1] = 0;
	if(use_mlock != 0 && mlock(ptr->long_state, hashMemSize) != 0)
		msg->warning = "";
	else
		ptr->ctx_info[1] = 1;

	return ptr;
#endif // _WIN32
}

void cryptonight_free_ctx(cryptonight_ctx* ctx)
{
	size_t hashMemSize = std::max(
		cn_select_memory(::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgo()),
		cn_select_memory(::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgoRoot())
	);

	if(ctx->ctx_info[0] != 0)
	{
#ifdef _WIN32
		VirtualFree(ctx->long_state, 0, MEM_RELEASE);
#else
		if(ctx->ctx_info[1] != 0)
			munlock(ctx->long_state, hashMemSize);
		munmap(ctx->long_state, hashMemSize);
#endif // _WIN32
	}
	else
		_mm_free(ctx->long_state);

	_mm_free(ctx);
}
