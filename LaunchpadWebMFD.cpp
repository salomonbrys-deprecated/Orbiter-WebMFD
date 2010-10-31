// ==============================================================
//                  ORBITER MODULE: WebMFD
//            Copyright (C) 2010 Salomon BRYS
//            Copyright (C) 2006 Martin Schweiger
//                   All rights reserved
//
// ServerControl.cpp
//
// Class implementation for ServerControl. Defines the properties and
// state of the web server in a dialog box.
// ==============================================================


#include "LaunchpadWebMFD.h"
#include "resource.h"
#include <sstream>
#include <atlimage.h>
#include <stdlib.h>

extern LaunchpadWebMFD *item;

BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		item->InitDialog(hDlg);
		break;
	case WM_CLOSE:
		item->CloseDialog(hDlg);
		break;
	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDC_EDIT:
			item->setText(hDlg);
			break;
		case IDC_OK:
			item->Apply(hDlg);
		}
	}
	return 0;
}


LaunchpadWebMFD::LaunchpadWebMFD(HINSTANCE hDLL) : hModule(hDLL), LaunchpadItem()
{
	_port = 8042;

	FILEHANDLE hFile = oapiOpenFile("Modules\\WebMFD.cfg", FILE_IN, CONFIG);
	if (!hFile)
		return ;

	int port;
	if (oapiReadItem_int(hFile, "PORT", port))
		_port = port;

	oapiCloseFile (hFile, FILE_IN);
}
	
char * LaunchpadWebMFD::Name()
{
	return "WebMFD Configuration";
}

char * LaunchpadWebMFD::Description()
{
	return "Configure the MFDs accessible from a Web browser";
}

bool LaunchpadWebMFD::clbkOpen(HWND hLaunchpad)
{
	return OpenDialog(hModule, hLaunchpad, IDD_MFD, DlgProc);
}

int LaunchpadWebMFD::clbkWriteConfig() 
{
	FILEHANDLE hFile = oapiOpenFile ("Modules\\WebMFD.cfg", FILE_OUT, CONFIG);
	if (!hFile)
		return 1;
	
	oapiWriteItem_int(hFile, "PORT", _port);
	oapiCloseFile(hFile, FILE_OUT);
	return 0;
}

void LaunchpadWebMFD::InitDialog(HWND hDlg)
{
	char portStr[6];
	_itoa_s(_port, portStr, 6, 10);

	SetWindowText(GetDlgItem(hDlg, IDC_EDIT), portStr);
	setText(hDlg);
}

void LaunchpadWebMFD::CloseDialog(HWND hDlg)
{
	EndDialog(hDlg, 0);
}

void LaunchpadWebMFD::Apply(HWND hDlg)
{
	char portStr[6];
	GetWindowText(GetDlgItem(hDlg, IDC_EDIT), portStr, 6);
	int port = atoi(portStr);
	if (port > 0 && port <= 65535)
	{
		_port = port;
		CloseDialog(hDlg);
	}
}

void	LaunchpadWebMFD::setText(HWND hDlg)
{
	char portStr[6];
	GetWindowText(GetDlgItem(hDlg, IDC_EDIT), portStr, 6);
	int port = atoi(portStr);

	std::stringstream sstr;
	sstr << "WebMFDs can be access on:" << std::endl;
	sstr << "http://localhost:" << port << std::endl;

	char ac[80];
	if (gethostname(ac, sizeof(ac)) != SOCKET_ERROR)
	{
		sstr << "http://" << ac << ':' << port << std::endl;
		struct hostent *phe = gethostbyname(ac);
		if (phe != 0)
			for (int i = 0; phe->h_addr_list[i] != 0; ++i)
			{
				struct in_addr addr;
				memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
				sstr << "http://" << inet_ntoa(addr) << ':' << port << std::endl;
			}
	}

	SetWindowText(GetDlgItem(hDlg, IDC_TEXT), sstr.str().c_str());
}
