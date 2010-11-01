/// \file
/// This file is inspired from http://www.ibm.com/developerworks/aix/library/au-endianc/index.html?ca=drs-
/// by Harsha S. Adiga

#include <Windows.h>

static const int i = 1;

/// Returns 1 if the running machine is big endian and 0 if not
#define is_bigendian() ( (*(char*)&i) == 0 )

/// Returns the big endian version of the given int, regardless of the running architecture.
INT32 getBigEndian(INT32 i)
{
	unsigned char c1, c2, c3, c4;

	if (is_bigendian())
		return i;

	c1 = i & 255;
	c2 = (i >> 8) & 255;
	c3 = (i >> 16) & 255;
	c4 = (i >> 24) & 255;

	return ((INT32)c1 << 24) + ((INT32)c2 << 16) + ((INT32)c3 << 8) + c4;
}
