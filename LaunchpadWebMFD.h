/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#ifndef __LAUNCHPAD_WEB_MFD_H
#define __LAUNCHPAD_WEB_MFD_H

#define STRICT 1
#include "Server.h"
#include "orbitersdk.h"

#include <windows.h>

/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Class interface for ServerControl.
/// Defines the properties and state of the web server in a dialog box.
/// This code is inspired by ExtMFD module by Martin Schweiger
class LaunchpadWebMFD : public LaunchpadItem
{
public:
	/// Constructor
	/// \param[in]	hDll	The Web MFD module DLL
	LaunchpadWebMFD(HINSTANCE hDLL);
	
	/// Orbiter LaunchpadItem callback that returns the name of the configuration dialog
	/// \note Note to Martin Schweiger: This method should be const !
	virtual char * Name();
	
	/// Orbiter LaunchpadItem callback that returns the description of the configuration dialog
	/// \note Note to Martin Schweiger: This method should be const !
	virtual char * Description();
	
	/// Orbiter LaunchpadItem callback that must open the configuration dialog
	virtual bool clbkOpen(HWND hLaunchpad);

	/// Orbiter LaunchpadItem callback that saves the configuration
	virtual int clbkWriteConfig();
	
	/// Initializes the dialog content
	/// \param[in]	hDlg	The dialog handle
	void InitDialog (HWND hDlg);

	/// Closes the dialog content
	/// \param[in]	hDlg	The dialog handle
	void CloseDialog(HWND hDlg);

	/// Saves the configuration
	/// \param[in]	hDlg	The dialog handle
	void Apply(HWND hDlg);
	
	/// Changes the text that gives the different URLs to access the web server
	/// \param[in]	hDlg	The dialog handle
	void setText(HWND hDlg);

	/// Get the port on which the server will be running
	/// \return The port on which the server will be running
	int Port() { return _port; }

private:
	/// The Web MFD module DLL
	HINSTANCE	hModule;
	
	/// The port on which the server will be running
	int _port;
};

#endif // __LAUNCHPAD_WEB_MFD_H
