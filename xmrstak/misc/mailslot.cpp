#include <windows.h>
#include <stdio.h>

BOOL WriteSlot(LPTSTR lpszMessage)
{
    LPTSTR SlotName = TEXT("\\\\.\\mailslot\\xmr-stak");
    HANDLE hFile = CreateFile(SlotName, GENERIC_WRITE, FILE_SHARE_READ, (LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
    DWORD cbWritten;
    return WriteFile(hFile, lpszMessage, (DWORD)(lstrlen(lpszMessage)+1) * sizeof(TCHAR), &cbWritten, (LPOVERLAPPED)NULL);
}