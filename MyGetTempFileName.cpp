// This code is inspired by the MSDN
// http://msdn.microsoft.com/en-us/library/aa363875(v=VS.85).aspx
// The filePath parameter must be able to receive MAX_PATH characters

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <string>

BOOL MyGetTempFileName(const char *prefix, char *filePath)
{
	TCHAR lpTempPathBuffer[MAX_PATH];
	DWORD dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer);
	if (dwRetVal > MAX_PATH || (dwRetVal == 0))
		return FALSE;

	UINT uRetVal = GetTempFileName(lpTempPathBuffer, prefix, 0, filePath);
	if (uRetVal == 0)
		return NULL;

	return true;
}
