/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#include "Server.h"
#include "ServerMFD.h"

#include <sstream>
#include <gdiplus.h>
#include <limits.h>

BOOL MyGetTempFileName(const char *prefix, char *filePath);


ServerMFD::ServerMFD() :
	// Initializes the specs and the ExternMFD with those specs
	_spec(0, 0, 255, 255, 6, 6, 255 / 7, (255 * 2) / 13), ExternMFD(_spec),
	// Default values for all properties
	_png(0), _jpeg(0), _nox(0), _btnProcess(false), _fileId(1), _btnClose(false)
{
	Resize(_spec);

	char	tmp[MAX_PATH];
	MyGetTempFileName("ORB", tmp);
	_tempFileName = tmp;

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	_btnMutex = CreateMutex(NULL, FALSE, NULL);
	_fileMutex = CreateMutex(NULL, FALSE, NULL);
}


ServerMFD::~ServerMFD(void)
{
	DeleteFile((_tempFileName + ".png").c_str());
	DeleteFile((_tempFileName + ".jpeg").c_str());

	CloseHandle(_btnMutex);
}


void ServerMFD::clbkRefreshDisplay(SURFHANDLE hSurf)
{
	if (!hSurf)
		return ;

	HDC hDCsrc = oapiGetDC(hSurf);

	HDC cdc = CreateCompatibleDC(hDCsrc);
	HBITMAP cbm = CreateCompatibleBitmap(hDCsrc, Width(), Height());
	HBITMAP oldbm = (HBITMAP)SelectObject(cdc, cbm);
	BitBlt(cdc, 0, 0, Width(), Height(), hDCsrc, 0, 0, SRCCOPY);
	SelectObject(cdc, oldbm);

	CImage img;
	img.Attach(cbm);

	WaitForSingleObject(_fileMutex, INFINITE);
	if (_png > 0)
	 	img.Save(_T((_tempFileName + ".png").c_str()), Gdiplus::ImageFormatPNG);
	if (_jpeg > 0)
	 	img.Save(_T((_tempFileName + ".jpeg").c_str()), Gdiplus::ImageFormatJPEG);
	_fileId = (_fileId + 1) % UINT_MAX;
	ReleaseMutex(_fileMutex);

	oapiReleaseDC(hSurf, hDCsrc);
}


HANDLE ServerMFD::getFileIf(const std::string &format, unsigned int &prevId)
{
	WaitForSingleObject(_fileMutex, INFINITE);
	if (prevId == _fileId)
	{
		ReleaseMutex(_fileMutex);
		return INVALID_HANDLE_VALUE;
	}

	if (format == "png")
	{
		if (_png <= 0)
		{
			ReleaseMutex(_fileMutex);
			return INVALID_HANDLE_VALUE;
		}
	}
	else if (format == "jpeg")
	{
		if (_jpeg <= 0)
		{
			ReleaseMutex(_fileMutex);
			return INVALID_HANDLE_VALUE;
		}
	}
	else
	{
		ReleaseMutex(_fileMutex);
		return INVALID_HANDLE_VALUE;
	}

	prevId = _fileId;
	HANDLE ret = CreateFile((_tempFileName + "." + format).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (ret == INVALID_HANDLE_VALUE)
		ReleaseMutex(_fileMutex);
	return ret;
}


void ServerMFD::closeFile(HANDLE file)
{
	CloseHandle(file);
	ReleaseMutex(_fileMutex);
}


void	ServerMFD::waitForBtnProcess()
{
	for (;;)
	{
		WaitForSingleObject(_btnMutex, INFINITE);
		if (!_btnProcess)
		{
			ReleaseMutex(_btnMutex);
			return ;
		}
		
		ReleaseMutex(_btnMutex);
		Sleep(20);
	}
}


void	ServerMFD::startBtnProcess()
{
	for (;;)
	{
		WaitForSingleObject(_btnMutex, INFINITE);
		if (!_btnProcess)
		{
			_btnProcess = true;
			ReleaseMutex(_btnMutex);
			return ;
		}
		
		ReleaseMutex(_btnMutex);
		Sleep(20);
	}
}


void	ServerMFD::endBtnProcess(int btnId)
{
	WaitForSingleObject(_btnMutex, INFINITE);
	_btnProcess = false;

	if (btnId == 13)
		SendKey (OAPI_KEY_F1);
	else if (btnId == 14)
		SendKey(OAPI_KEY_GRAVE);
	else if (btnId == 99)
	{
		SendKey(OAPI_KEY_ESCAPE);
		_btnClose = true;
		WaitForSingleObject(_fileMutex, INFINITE);
		++_fileId;
		ReleaseMutex(_fileMutex);
	}
	else
	{
		ProcessButton(btnId, PANEL_MOUSE_LBDOWN);
		ProcessButton(btnId, PANEL_MOUSE_LBUP);
	}

	_generateJSON();

	ReleaseMutex(_btnMutex);
}


std::string	ServerMFD::getJSON()
{
	WaitForSingleObject(_btnMutex, INFINITE);
	std::string ret = _JSON;
	ReleaseMutex(_btnMutex);
	return ret;
}


bool	ServerMFD::getClose()
{
	WaitForSingleObject(_btnMutex, INFINITE);
	bool ret = _btnClose;
	ReleaseMutex(_btnMutex);
	return ret;
}


void ServerMFD::_generateJSON()
{
	_JSON = "{ \"left\": [ \"";
	for (int i = 0; i < 6; ++i)
	{
		const char * label = GetButtonLabel(i);
		if (label)
			_JSON += label;
		if (i < 5)
			_JSON += "\", \"";
	}
	_JSON += "\" ], \"right\": [ \"";
	for (int i = 6; i < 12; ++i)
	{
		const char * label = GetButtonLabel(i);
		if (label)
			_JSON += label;
		if (i < 11)
			_JSON += "\", \"";
	}
	_JSON += "\" ] }";
}
