
#include <Windows.h>

//inline bool isLittleEndian()
//{
//	int i = 1;
//	char *p = (char *)&i;
//
//	if (p[0] == 1)
//		return true;
//	else
//		return false;
//}
//
//inline void endian_swap(INT32& x)
//{
//    x = (x>>24) | 
//        ((x<<8) & 0x00FF0000) |
//        ((x>>8) & 0x0000FF00) |
//        (x<<24);
//}

// The following code is inspired from
// http://www.ibm.com/developerworks/aix/library/au-endianc/index.html?ca=drs-

static const int i = 1;
#define is_bigendian() ( (*(char*)&i) == 0 )

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
