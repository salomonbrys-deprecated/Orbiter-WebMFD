/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#include "Server.h"
#include "ServerMFD.h"

#include <gdiplus.h>
#include <limits.h>

BOOL MyGetTempFileName(const char *prefix, char *filePath);


ServerMFD::ServerMFD() :
	// Initializes the specs and the ExternMFD with those specs
	_spec(0, 0, 255, 255, 6, 6, 255 / 7, (255 * 2) / 13), ExternMFD(_spec),
	// Default values for all properties
	_png(0), _jpeg(0), _nox(0), _surfaceId(1), _surfaceHasChanged(false), _btnLabelsId(1), _btnClose(false)
{
	// First thing a MFD needs to do
	Resize(_spec);

	// Get a temporary file path for the images
	char	tmp[MAX_PATH];
	MyGetTempFileName("ORB", tmp);
	_tempFileName = tmp;

	// Start GDI
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Create the mutexes
	_btnMutex = CreateMutex(NULL, FALSE, NULL);
	_fileMutex = CreateMutex(NULL, FALSE, NULL);
	_imageMutex = CreateMutex(NULL, FALSE, NULL);

	// Create the semaphore
	_btnProcessSmp = CreateSemaphore(NULL, 1, 1, NULL);

	// Create the surface
	_surface = oapiCreateSurface(255, 255);
}


ServerMFD::~ServerMFD(void)
{
	// Delete the temporary images files
	DeleteFile((_tempFileName + ".png").c_str());
	DeleteFile((_tempFileName + ".jpeg").c_str());

	// Destroy the mutexes
	CloseHandle(_btnMutex);
	CloseHandle(_btnProcessSmp);
	CloseHandle(_fileMutex);
	CloseHandle(_imageMutex);
}


void ServerMFD::clbkRefreshDisplay(SURFHANDLE hSurf)
{
	// If the given surface is 0, just retrun
	if (!hSurf)
		return ;

	WaitForSingleObject(_imageMutex, INFINITE);
	oapiBlt(_surface, hSurf, 0, 0, 0, 0, Width(), Height());
	_surfaceHasChanged = true;
	ReleaseMutex(_imageMutex);
}

void ServerMFD::clbkRefreshButtons ()
{
	// Regenerate the JSON string
	_generateJSON();
}

HANDLE ServerMFD::getFileIf(const std::string &format, unsigned int &prevId)
{
	// Wait to be able to access the image files by waiting to gain acces to their mutex
	WaitForSingleObject(_fileMutex, INFINITE);

	// If the image need to be regenerated, do it
	if (_surfaceHasChanged)
	{
		// Wait to be able to access the image by waiting to gain acces to its mutex
		WaitForSingleObject(_imageMutex, INFINITE);

		// Copy the MFD surface to bitmap
		_copySurfaceToBitmap();

		// Release the image files access mutex
		ReleaseMutex(_imageMutex);

		_generateImage();
	}

	// If the request gave the same id as the current id,
	// it means that the image has not changed since last request.
	if (prevId == _surfaceId)
	{
		// Release the image files access mutex
		ReleaseMutex(_fileMutex);

		// Return an invalid handle because the image has not changed
		return INVALID_HANDLE_VALUE;
	}

	// If the image is requested using the PNG format
	if (format == "png")
	{
		// If there are no PNG follower
		if (_png <= 0)
		{
			// Release the image files access mutex
			ReleaseMutex(_fileMutex);

			// Return an invalid handle because the image has not been generated in PNG as there are no PNG followers
			return INVALID_HANDLE_VALUE;
		}
	}
	// If the image is requested using the JPEG format
	else if (format == "jpeg")
	{
		// If there are no JPEG follower
		if (_jpeg <= 0)
		{
			// Release the image files access mutex
			ReleaseMutex(_fileMutex);

			// Return an invalid handle because the image has not been generated in JPEG as there are no JPEG followers
			return INVALID_HANDLE_VALUE;
		}
	}
	// The image is requested using an unknown format
	else
	{
		// Release the image files access mutex
		ReleaseMutex(_fileMutex);

		// Return an invalid handle because the image has been requested in an unknown format
		return INVALID_HANDLE_VALUE;
	}

	// Update the given id reference
	prevId = _surfaceId;

	// Open the requested image file
	HANDLE ret = CreateFile((_tempFileName + "." + format).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	// If the requested image file could not be open, release the image files access mutex
	// If the requested image file could be open, then the mutex is kept until closeFile is called
	if (ret == INVALID_HANDLE_VALUE)
		ReleaseMutex(_fileMutex);

	// Return the file handle
	return ret;
}


void ServerMFD::closeFile(HANDLE file)
{
	// Closing the file handle
	CloseHandle(file);

	// Release the image files access mutex, enabling other thread to get the image and to update id
	ReleaseMutex(_fileMutex);
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

		// Update the file id
		// The image has not been updated but the file id is still incremented to force the refresh on all folowers,
		// which will un-freeze them and enable to check getClose.
		// Because the MFD has been shutdown, the image will no longer be refreshed and the id will no longer change.
		WaitForSingleObject(_fileMutex, INFINITE);
		++_surfaceId;
		ReleaseMutex(_fileMutex);
	}

	// The button pressed is a regular button
	else
	{
		// Send the click (MOUSEDOWN + MOUSEUP) event
		ProcessButton(btnId, PANEL_MOUSE_LBDOWN);
		ProcessButton(btnId, PANEL_MOUSE_LBUP);
	}

	// Release the button process mutex, meaning the end of the button process
	ReleaseSemaphore(_btnProcessSmp, 1, NULL);
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
		ReleaseMutex(_fileMutex);

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
	_bmpFromSurface = CreateCompatibleBitmap(hDCsrc, Width(), Height());
	HBITMAP oldbm = (HBITMAP)SelectObject(cdc, _bmpFromSurface);
	BitBlt(cdc, 0, 0, Width(), Height(), hDCsrc, 0, 0, SRCCOPY);
	SelectObject(cdc, oldbm);

	// Release the Surface Device Context
	oapiReleaseDC(_surface, hDCsrc);
}

void ServerMFD::_generateImage()
{
	// Create an image from the bitmap
	CImage img;
	img.Attach(_bmpFromSurface);

	// If the MFD have any PNG followers, then saves the image into the png temporary image file
	if (_png > 0)
	 	img.Save(_T((_tempFileName + ".png").c_str()), Gdiplus::ImageFormatPNG);

	// If the MFD have any JPEG followers, then saves the image into the jpeg temporary image file
	if (_jpeg > 0)
	 	img.Save(_T((_tempFileName + ".jpeg").c_str()), Gdiplus::ImageFormatJPEG);

	// Incrementing the image id, modulo the maximum possible value for a int
	_surfaceId = ((_surfaceId + 1) % (UINT_MAX - 1)) + 1;

	// No need to regenrate until the surface changes
	_surfaceHasChanged = false;
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
