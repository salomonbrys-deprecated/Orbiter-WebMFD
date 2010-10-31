// ==============================================================
//                  ORBITER MODULE: WebMFD
//            Copyright (C) 2010 Salomon BRYS
//            Copyright (C) 2006 Martin Schweiger
//                   All rights reserved
//
// WebMFD.cpp
//
// Creates a web server to access MFDs in any web browser.
// ==============================================================

#define STRICT 1
#define ORBITER_MODULE
#include "Server.h"
#include "LaunchpadWebMFD.h"
#include "orbitersdk.h"
#include "resource.h"

#include <windows.h>
#include <sstream>

LaunchpadWebMFD *item;

class WebMFDModule : public oapi::Module
{
public:
	WebMFDModule(HINSTANCE hDLL) : oapi::Module(hDLL)
	{
		_hDLL = hDLL;
	}

	virtual void clbkSimulationStart(RenderMode)
	{
		Server::Instance().start(8042);
	}

	virtual void clbkSimulationEnd()
	{
		Server::Instance().stop();
	}

	virtual void clbkPreStep(double, double, double)
	{
		Server::Instance().clbkOrbiter();
	}


private:
	HINSTANCE _hDLL;
};

DLLCLBK void opcDLLInit(HINSTANCE hDLL)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);

	oapiRegisterModule(new WebMFDModule(hDLL));

	item = new LaunchpadWebMFD(hDLL);
	oapiRegisterLaunchpadItem(item);
}

DLLCLBK void opcDLLExit(HINSTANCE hDLL)
{
	oapiUnregisterLaunchpadItem(item);
	delete item;
}
