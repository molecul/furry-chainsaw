#include <windows.h>
#include <stdio.h>

LPTSTR SlotName = TEXT("\\\\.\\mailslot\\xmr-stak")

BOOL WriteSlot(HANDLE hSlot, LPTSTR lpszMessage)
{
    DWORD cbWritten;
    return WriteFile(hSlot, lpszMessage, (DWORD)(lstrlen(lpszMessage)+1) * sizeof(TCHAR), &cbWritten, (LPOVERLAPPED)NULL);
}