/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#define STRICT 1
#define ORBITER_MODULE
#include "Server.h"
#include "LaunchpadWebMFD.h"
#include "orbitersdk.h"
#include "resource.h"

#include <windows.h>

LaunchpadWebMFD *item;

/// WebMFD Module Class: Handles the WebMFD plugin
/// Creates a web server to access MFDs in any web browser.
class WebMFDModule : public oapi::Module
{
public:
	/// Constructor
	/// \param[in]	hDLL	The WebMFD module DLL
	WebMFDModule(HINSTANCE hDLL) : oapi::Module(hDLL)
	{
		// Save the WebMFD module DLL
		_hDLL = hDLL;

		// Initialize the Windows Socket API
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2,2), &wsaData);
	}

	/// Orbiter callback to be called when the simulation starts
	/// Starts the server
	/// The parameter is ignored because unused
	virtual void clbkSimulationStart(RenderMode)
	{
		Server::Instance().start(8042);
	}

	/// Orbiter callback to be called when the simulation ends
	/// Stops the server
	virtual void clbkSimulationEnd()
	{
		Server::Instance().stop();
	}

	/// Orbiter callback to be called before each state is updated
	/// Launch the registered MFD treatments
	/// The parameter are ignored because unused
	virtual void clbkPreStep(double, double, double)
	{
		Server::Instance().clbkOrbiterPreStep();
	}

private:
	/// The WebMFD module DLL
	HINSTANCE _hDLL;
};


/// Module function that is called by Orbiter to initialize the plugin
/// Registers the WebMFD Module Class and the launchpad configuration item
DLLCLBK void opcDLLInit(HINSTANCE hDLL)
{
	// Register the WebMFD Module Class
	oapiRegisterModule(new WebMFDModule(hDLL));

	// Register the launchpad configuration item
	item = new LaunchpadWebMFD(hDLL);
	oapiRegisterLaunchpadItem(item);
}


/// Module function that is called by Orbiter to close the plugin
/// Unregisters the launchpad configuration item
/// The WebMFD Module Class will be destroyed by Orbiter as it as been registered using oapiRegisterModule
DLLCLBK void opcDLLExit(HINSTANCE hDLL)
{
	oapiUnregisterLaunchpadItem(item);
	delete item;
}
