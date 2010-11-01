/// \file
/// This file is inspired from http://msdn.microsoft.com/en-us/library/aa363875(v=VS.85).aspx
/// by Microsoft (c)

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <string>

/// Gets a temporary file absolute path.
/// \param[in]	prefix		The prefix of the temporary file name to create.
/// \param[out]	filePath	The memory on which the path will be written, must be able to receive MAX_PATH characters.
/// \return Wether the temporary file creation has succeeded or not.
BOOL MyGetTempFileName(const char *prefix, char *filePath)
{
	TCHAR lpTempPathBuffer[MAX_PATH];
	DWORD dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer);
	if (dwRetVal > MAX_PATH || (dwRetVal == 0))
		return FALSE;

	UINT uRetVal = GetTempFileName(lpTempPathBuffer, prefix, 0, filePath);
	if (uRetVal == 0)
		return NULL;

	return TRUE;
}
