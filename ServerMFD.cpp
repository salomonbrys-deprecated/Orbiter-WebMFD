/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#include "Server.h"
#include "ServerMFD.h"

#include <gdiplus.h>
#include <limits.h>

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

ServerMFD::ServerMFD(const std::string & key) :
	// Initializes the specs and the ExternMFD with those specs
	_spec(0, 0, 255, 255, 6, 6, 255 / 7, (255 * 2) / 13), ExternMFD(_spec),
	// Default values for all properties
	_pngFollowers(0), _jpegFollowers(0), _noxFollowers(0), _surface(0), _surfaceId(0), _surfaceHasChanged(false), _btnLabelsId(1), _btnClose(false)
{
	// First thing a MFD needs to do
	Resize(_spec);

	// Start GDI
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Create the mutexes
	_btnMutex = CreateMutex(NULL, FALSE, NULL);
	_streamMutex = CreateMutex(NULL, FALSE, NULL);
	_imageMutex = CreateMutex(NULL, FALSE, NULL);

	// Create the semaphore
	_btnProcessSmp = CreateSemaphore(NULL, 1, 1, NULL);

	// Create the surface
	_surface = oapiCreateSurface(255, 255);

	// Load a compatible bitmap in order to get a compatible HBITMAP (...no comment...)
	_bmpFromSurface = (HBITMAP)LoadImage(NULL, _T("WebMFD\\_testimage.bmp"), IMAGE_BITMAP, 0,0, LR_LOADFROMFILE);
}


ServerMFD::~ServerMFD(void)
{
	// Destroy the mutexes
	CloseHandle(_btnMutex);
	CloseHandle(_btnProcessSmp);
	CloseHandle(_streamMutex);
	CloseHandle(_imageMutex);

	// Delete the PNG stream and it's allocated memory
	if (_streamPNG.stream)
		_streamPNG.stream->Release();

	// Delete the JPEG stream and it's allocated memory
	if (_streamJPEG.stream)
		_streamJPEG.stream->Release();

}


void ServerMFD::clbkRefreshDisplay(SURFHANDLE hSurf)
{
	// If the given surface is 0, just retrun
	if (!hSurf)
		return ;

	// Wait to be able to access the image surface by waiting to gain acces to its mutex
	WaitForSingleObject(_imageMutex, INFINITE);

	// Save the given surface to a local surface that is manipulable by other threads
	oapiBlt(_surface, hSurf, 0, 0, 0, 0, Width(), Height());

	// State that the surface has changed (trigering, in a follower thread, the image generation)
	_surfaceHasChanged = true;

	// Release the image surface access mutex
	ReleaseMutex(_imageMutex);
}

void ServerMFD::clbkRefreshButtons ()
{
	// Regenerate the JSON string
	_generateJSON();
}

imageStream * ServerMFD::getStreamIf(const std::string &format, unsigned int &prevId)
{
	// Wait to be able to access the image buffers by waiting to gain acces to their mutex
	WaitForSingleObject(_streamMutex, INFINITE);

	// If the image need to be regenerated, do it
	if (_surfaceHasChanged)
	{
		// Wait to be able to access the image surface by waiting to gain acces to its mutex
		WaitForSingleObject(_imageMutex, INFINITE);

		// Copy the MFD surface to bitmap
		_copySurfaceToBitmap();

		// Release the image surface access mutex
		ReleaseMutex(_imageMutex);

		// Generate the image from HBITMAP
		_generateImage();
	}

	// If the request gave the same id as the current id,
	// it means that the image has not changed since last request.
	if (prevId == _surfaceId)
	{
		// Release the image buffers access mutex
		ReleaseMutex(_streamMutex);

		// Return an invalid handle because the image has not changed
		return 0;
	}

	// What will be returned
	imageStream * ret = 0;

	// If the image is requested using the PNG format
	if (format == "png")
	{
		// If there are no PNG follower or if the PNG stream has not been allocated
		if (_pngFollowers <= 0 || !_streamPNG.stream)
		{
			// Release the image buffers access mutex
			ReleaseMutex(_streamMutex);

			// Return an invalid handle because the image has not been generated in PNG as there are no PNG followers
			return 0;
		}

		// The PNG buffer will be returned
		ret = &_streamPNG;
	}
	// If the image is requested using the JPEG format
	else if (format == "jpeg" || !_streamJPEG.stream)
	{
		// If there are no JPEG follower or if the PNG stream has not been allocated
		if (_jpegFollowers <= 0)
		{
			// Release the image buffers access mutex
			ReleaseMutex(_streamMutex);

			// Return an invalid handle because the image has not been generated in JPEG as there are no JPEG followers
			return 0;
		}

		// The JPEG buffer will be returned
		ret = &_streamJPEG;
	}
	// The image is requested using an unknown format
	else
	{
		// Release the image buffers access mutex
		ReleaseMutex(_streamMutex);

		// Return an invalid handle because the image has been requested in an unknown format
		return 0;
	}

	// Update the given id reference
	prevId = _surfaceId;

	// Set the stream cursor at the beginning
	LARGE_INTEGER moveBy;
	moveBy.QuadPart = 0;
	ret->stream->Seek(moveBy, STREAM_SEEK_SET, NULL);

	// Return the file handle
	return ret;
}


void ServerMFD::closeStream()
{
	// Release the image buffers access mutex, enabling other thread to get the image and to update id
	ReleaseMutex(_streamMutex);
}


void	ServerMFD::waitForBtnProcess()
{
	// Wait for the button process mutex to be released, which means that no button process is running
	WaitForSingleObject(_btnProcessSmp, INFINITE);

	// Release the button process mutex as the lock was just to wait for its release
	ReleaseSemaphore(_btnProcessSmp, 1, NULL);
}


void	ServerMFD::startBtnProcess()
{
	// Acquire a lock on the button process mutex, which means that a button process starts
	WaitForSingleObject(_btnProcessSmp, INFINITE);
}

void	ServerMFD::endBtnProcess()
{
	// Release the button process mutex, meaning the end of the button process
	ReleaseSemaphore(_btnProcessSmp, 1, NULL);
}

void	ServerMFD::execBtnProcess(int btnId)
{
	// If the button id is 13, it is the SEL key, send the corresponding event
	if (btnId == 13)
		SendKey (OAPI_KEY_F1);

	// If the button id is 14, it is the MNU key, send the corresponding event
	else if (btnId == 14)
		SendKey(OAPI_KEY_GRAVE);

	// If the button id is 99, it is the PWR key
	else if (btnId == 99)
	{
		// Send the close event
		SendKey(OAPI_KEY_ESCAPE);

		// Register the fact that the close button has been pressed
		WaitForSingleObject(_btnMutex, INFINITE);
		_btnClose = true;
		ReleaseMutex(_btnMutex);

		// Update the surface id
		// The image has not been updated but the surface id is still incremented to force the refresh on all folowers,
		// which will un-freeze them and enable to check getClose.
		// Because the MFD has been shutdown, the image will no longer be refreshed and the id will no longer change.
		WaitForSingleObject(_streamMutex, INFINITE);
		++_surfaceId;
		ReleaseMutex(_streamMutex);
	}

	// The button pressed is a regular button
	else
	{
		// Send the click (MOUSEDOWN + MOUSEUP) event
		ProcessButton(btnId, PANEL_MOUSE_LBDOWN);
		ProcessButton(btnId, PANEL_MOUSE_LBUP);
	}
}


std::string	ServerMFD::getJSON()
{
	// Wait to be able to access the button informations by waiting to gain acces to their mutex
	WaitForSingleObject(_btnMutex, INFINITE);

	// Copy the JSON informations in a temporary variable as we need to release the mutex before returning
	std::string ret = _JSON;

	// Release the button informations access mutex
	ReleaseMutex(_btnMutex);

	// Return the JSON button informations
	return ret;
}


std::string	ServerMFD::getJSONIf(unsigned int &prevId)
{
	// Wait to be able to access the button informations by waiting to gain acces to their mutex
	WaitForSingleObject(_btnMutex, INFINITE);

	// If the request gave the same id as the current id,
	// it means that the buttons labels have not changed since last request.
	if (prevId == _btnLabelsId)
	{
		// Release the button informations access mutex
		ReleaseMutex(_btnMutex);

		// Return an empty string because the buttons labels have not changed
		return "";
	}

	// Copy the JSON informations in a temporary variable as we need to release the mutex before returning
	std::string ret = _JSON;

	// Release the button informations access mutex
	ReleaseMutex(_btnMutex);

	// Return the JSON button informations
	return ret;
}


bool	ServerMFD::getClose()
{
	// Wait to be able to access the button informations by waiting to gain acces to their mutex
	WaitForSingleObject(_btnMutex, INFINITE);

	// Copy the button close information in a temporary variable as we need to release the mutex before returning
	bool ret = _btnClose;

	// Release the button informations access mutex
	ReleaseMutex(_btnMutex);

	// Return the button close information
	return ret;
}

void ServerMFD::_copySurfaceToBitmap()
{
	// Get the Device Context from the Surface
	HDC hDCsrc = oapiGetDC(_surface);

	// Copy the Device Context into a Bitmap
	HDC cdc = CreateCompatibleDC(hDCsrc);	
	HBITMAP oldbm = (HBITMAP)SelectObject(cdc, _bmpFromSurface);
	BitBlt(cdc, 0, 0, Width(), Height(), hDCsrc, 0, 0, SRCCOPY);
	SelectObject(cdc, oldbm);

	// Release the Surface Device Context
	oapiReleaseDC(_surface, hDCsrc);
	DeleteDC(cdc);

	// No need to regenrate until the surface changes
	_surfaceHasChanged = false;
}


void ServerMFD::_generateImage()
{
	// Create a gdiplus image from the bitmap
	Gdiplus::Bitmap * img = Gdiplus::Bitmap::FromHBITMAP(_bmpFromSurface, NULL);

	// Argument for later Seek call
	LARGE_INTEGER moveBy;
	moveBy.QuadPart = 0;

	// If the MFD have any PNG followers, then saves the image into the png image stream
	if (_pngFollowers > 0)
	{
		// If the PNG stream does not yet exists, allocates it
		if (!_streamPNG.stream)
		{
			_streamPNG.Memory = GlobalAlloc(GMEM_MOVEABLE, 1024 * 64);
			CreateStreamOnHGlobal(_streamPNG.Memory, TRUE, &_streamPNG.stream);
		}

		// Get the PNG encoder CLSID
		CLSID pngClsid;
		GetEncoderClsid(L"image/png", &pngClsid);

		// Move the stream cursor to the beginning of the stream
		_streamPNG.stream->Seek(moveBy, STREAM_SEEK_SET, NULL);

		// Save the image into the stream
		img->Save(_streamPNG.stream, &pngClsid);

		// Get the size of the image according to the new cursor position
		LARGE_INTEGER moveBy;
		moveBy.QuadPart = 0;
		ULARGE_INTEGER nPos;
		_streamPNG.stream->Seek(moveBy, STREAM_SEEK_CUR, &nPos);
		_streamPNG.size = nPos.QuadPart;
	}

	// If the MFD have any JPEG followers, then saves the image into the jpeg image stream
	if (_jpegFollowers > 0)
	{
		// If the JPEG stream does not yet exists, allocates it
		if (!_streamJPEG.stream)
		{
			_streamJPEG.Memory = GlobalAlloc(GMEM_MOVEABLE, 1024 * 64);
			CreateStreamOnHGlobal(_streamJPEG.Memory, TRUE, &_streamJPEG.stream);
		}

		// Get the JPEG encoder CLSID
		CLSID jpegClsid;
		GetEncoderClsid(L"image/jpeg", &jpegClsid);

		// Move the stream cursor to the beginning of the stream
		_streamJPEG.stream->Seek(moveBy, STREAM_SEEK_SET, NULL);

		// Save the image into the stream
		img->Save(_streamJPEG.stream, &jpegClsid);

		// Get the size of the image according to the new cursor position
		LARGE_INTEGER moveBy;
		moveBy.QuadPart = 0;
		ULARGE_INTEGER nPos;
		_streamJPEG.stream->Seek(moveBy, STREAM_SEEK_CUR, &nPos);
		_streamJPEG.size = nPos.QuadPart;
	}

	delete img;

	// Incrementing the image id, modulo the maximum possible value for a int
	_surfaceId = ((_surfaceId + 1) % (UINT_MAX - 1)) + 1;
}


void ServerMFD::_generateJSON()
{
	// Wait to be able to access the button informations by waiting to gain acces to their mutex
	WaitForSingleObject(_btnMutex, INFINITE);

	// Starting the JSON string with the left buttons
	_JSON = "{ \"left\": [ \"";

	// For each 6 left buttons
	for (int i = 0; i < 6; ++i)
	{
		// Get the button label
		const char * label = GetButtonLabel(i);

		// Add the label to the JSON string
		// If the label is 0 (which means that the button is unused), le string won't be append, leaving an empty string in the JSON string
		if (label)
			_JSON += label;

		// Adding the JSON separator if this is not the last left button
		if (i < 5)
			_JSON += "\", \"";
	}

	// Continuing  the JSON string with the right buttons
	_JSON += "\" ], \"right\": [ \"";

	// For each 6 right buttons
	for (int i = 6; i < 12; ++i)
	{
		// Get the button label
		const char * label = GetButtonLabel(i);

		// Add the label to the JSON string
		// If the label is 0 (which means that the button is unused), le string won't be append, leaving an empty string in the JSON string
		if (label)
			_JSON += label;

		// Adding the JSON separator if this is not the last right button
		if (i < 11)
			_JSON += "\", \"";
	}

	// Finishing the JSON string
	_JSON += "\" ] }";

	// Update the _btnLabelsId
	_btnLabelsId = ((_btnLabelsId + 1) % (UINT_MAX - 1)) + 1;

	// Release the button informations access mutex
	ReleaseMutex(_btnMutex);
}
