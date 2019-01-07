#include <windows.h>
#include <stdio.h>

#include "utility.hpp"
#include "executor.hpp"

#include "xmrstak/backend/iBackend.hpp"
#include "xmrstak/backend/backendConnector.hpp"

using namespace std;
using namespace xmrstak;

BOOL WriteSlot()
{
    LPTSTR SlotName = TEXT("\\\\.\\mailslot\\xmr-stak");
    HANDLE hFile = CreateFile(SlotName, GENERIC_WRITE, FILE_SHARE_READ, (LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
    DWORD cbWritten;
    double fTotal = 0.0;

    DWORD Pid = GetCurrentProcessId();

    size_t nthd = executor::inst()->nthd;
    bool connected = executor::inst()->poolConnected;
    std::cout << "Count of nthd: " << nthd << std::endl;

	xmrstak::telemetry* telem = executor::inst()->ct;

	for (size_t i = 0; i < nthd; i++)
	{
		fTotal += telem->calc_telemetry_data(10000, i);
        std::cout << "["<<i<<"] "<<"Current hashrate: " << fTotal << " H/s" << std::endl;
	}
	std::cout << "Current hashrate: " << fTotal << " H/s" << std::endl;

    char buff[100];
    sprintf(buff, "%f|%d|%d", fTotal, connected, Pid);

    LPTSTR finMsg = buff;
    return WriteFile(hFile, finMsg, (DWORD)(lstrlen(finMsg)+1) * sizeof(TCHAR), &cbWritten, (LPOVERLAPPED)NULL);
}