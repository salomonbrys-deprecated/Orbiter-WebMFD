/// \file
/// This file is inspired from 
/// by Microsoft (c)

#include <windows.h>
#include <gdiplus.h>

/// Receives the MIME type of an encoder and returns the class identifier (CLSID) of that encoder.
/// The MIME types of the encoders built into Windows GDI+ are as follows: image/bmp, image/jpeg, image/gif, image/tiff, image/png.
/// \param[in]	format		The mime-type format of the CLSID to get.
/// \param[out]	pClsid		A pointer to the CLSID to fill.
/// \return the index of the ImageCodecInfo object if success, -1 if failure
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if(size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if(pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for(UINT j = 0; j < num; ++j)
	{
		if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}    
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}
