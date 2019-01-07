#include <windows.h>
#include <stdio.h>

#include "utility.hpp"
#include "telemetry.hpp"

#include "xmrstak/backend/iBackend.hpp"

BOOL WriteSlot()
{
    LPTSTR SlotName = TEXT("\\\\.\\mailslot\\xmr-stak");
    HANDLE hFile = CreateFile(SlotName, GENERIC_WRITE, FILE_SHARE_READ, (LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
    DWORD cbWritten;

    xmrstak::miner_work oWork = xmrstak::miner_work();
	std::vector<xmrstak::iBackend*>* pvThreads = xmrstak::BackendConnector::thread_starter(oWork);
	xmrstak::telemetry* telem = new xmrstak::telemetry(pvThreads->size());
	size_t nthd = pvThreads->size();
	double fTotal = 0.0;

	fTotal = 0.0;
	for (size_t i = 0; i < nthd; i++)
	{
		fTotal += telem->calc_telemetry_data(10000, i);
	}
	std::cout << "Current hashrate: " << fTotal << " H/s" << std::endl;

    char *sTotal;
    LPTSTR = lpszMessage;
    sprintf(sTotal, "%f", fTotal);
    lpszMessage = TEXT(sTotal);
    return WriteFile(hFile, lpszMessage, (DWORD)(lstrlen(lpszMessage)+1) * sizeof(TCHAR), &cbWritten, (LPOVERLAPPED)NULL);
}