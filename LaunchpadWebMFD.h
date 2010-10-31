// ==============================================================
//                  ORBITER MODULE: WebMFD
//            Copyright (C) 2010 Salomon BRYS
//            Copyright (C) 2006 Martin Schweiger
//                   All rights reserved
//
// ServerControl.h
//
// Class interface for ServerControl. Defines the properties and
// state of the web server in a dialog box.
// This code is inspired by ExtMFD module by Martin Schweiger
// ==============================================================

#ifndef __LAUNCHPAD_WEB_MFD_H
#define __LAUNCHPAD_WEB_MFD_H

#define STRICT 1
#include "Server.h"
#include "orbitersdk.h"

#include <windows.h>

class LaunchpadWebMFD : public LaunchpadItem
{
public:
	LaunchpadWebMFD(HINSTANCE hDLL);
	virtual char * Name();
	virtual char * Description();
	virtual bool clbkOpen(HWND hLaunchpad);
	virtual int clbkWriteConfig();
	
	void InitDialog (HWND hDlg);
	void CloseDialog(HWND hDlg);
	void Apply(HWND hDlg);
	void setText(HWND hDlg);

	int Port() { return _port; }

private:
	HINSTANCE	hModule;
	int _port;
};

#endif // __LAUNCHPAD_WEB_MFD_H
