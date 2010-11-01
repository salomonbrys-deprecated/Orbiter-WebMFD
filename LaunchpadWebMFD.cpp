/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#include "LaunchpadWebMFD.h"
#include "resource.h"
#include <sstream>
#include <atlimage.h>
#include <stdlib.h>

/// The default port value if it has not been configured yet
#define WEBMFD_DEFAULT_PORT_VALUE	8042

/// The configuration file path
#define WEBMFD_CONF_FILE_PATH		"Modules\\WebMFD.cfg"

/// The LaunchpadWebMFD created in WebMFD.cpp
extern LaunchpadWebMFD *item;

/// The window callback that will handle the configuration dialog events
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		// The dialog has just been opened
		item->InitDialog(hDlg);
		break;
		
	case WM_CLOSE:
		// The close button has been clicked
		item->CloseDialog(hDlg);
		break;
		
	case WM_COMMAND:
		// A sub-control event has occured
		switch (LOWORD (wParam))
		{
		case IDC_EDIT:
			// The port text field has been edited
			item->setText(hDlg);
			break;
			
		case IDC_OK:
			// The apply button has been cliecked
			item->Apply(hDlg);
		}
	}
	return 0;
}


LaunchpadWebMFD::LaunchpadWebMFD(HINSTANCE hDLL) : hModule(hDLL), LaunchpadItem()
{
	// Set the port to its default value
	_port = WEBMFD_DEFAULT_PORT_VALUE;

	// Open the configuration file on read only
	FILEHANDLE hFile = oapiOpenFile(WEBMFD_CONF_FILE_PATH, FILE_IN, CONFIG);
	
	// If the configuration file does not exists, returns
	// The _port variable stays then at its default value
	if (!hFile)
		return ;

	// Read the port on the configuration file and set it
	int portTMP;
	if (oapiReadItem_int(hFile, "PORT", portTMP))
		_port = portTMP;

	// Closes the configuration file
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
	// Open the configuration file on write only
	FILEHANDLE hFile = oapiOpenFile("Modules\\WebMFD.cfg", FILE_OUT, CONFIG);

	// Aboard if the oapiOpenFile has failed
	if (!hFile)
		return 1;
	
	// Write the configuration
	oapiWriteItem_int(hFile, "PORT", _port);

	// Closes the configuration file
	oapiCloseFile(hFile, FILE_OUT);
	
	// Return success
	return 0;
}


void LaunchpadWebMFD::InitDialog(HWND hDlg)
{
	// Transform the port integer into a null-terminated char string
	char portStr[6];
	_itoa_s(_port, portStr, 6, 10);

	// Set the text on the text edit control
	SetWindowText(GetDlgItem(hDlg, IDC_EDIT), portStr);
	
	// Update the information text
	setText(hDlg);
}


void LaunchpadWebMFD::CloseDialog(HWND hDlg)
{
	EndDialog(hDlg, 0);
}


void LaunchpadWebMFD::Apply(HWND hDlg)
{
	// Read the port string of the dialog control
	char portStr[6];
	GetWindowText(GetDlgItem(hDlg, IDC_EDIT), portStr, 6);
	
	// Transform the port string to an integer
	int port = atoi(portStr);
	
	// If the given port is in the windows authorized port range
	if (port > 0 && port <= 65535)
	{
		// Save the port
		_port = port;
		
		// Close the dialog (which will fire the close event and provoque the saving of the configuration)
		CloseDialog(hDlg);
	}
}


void	LaunchpadWebMFD::setText(HWND hDlg)
{
	// Read the port string of the dialog control
	char portStr[6];
	GetWindowText(GetDlgItem(hDlg, IDC_EDIT), portStr, 6);

	// Transform the port string to an integer
	int port = atoi(portStr);

	// The string stream on which the information text will be pushed
	std::stringstream sstr;

	// Push the information text into the information text
	sstr << "WebMFDs can be access on:" << std::endl;
	sstr << "http://localhost:" << port << std::endl;

	// The null-terminated char string in which the main host name of the running machine will be written
	char ac[128];
	
	// If the reading of the host name has succeeded
	if (gethostname(ac, sizeof(ac)) != SOCKET_ERROR)
	{
		// Push the main URL into the information text
		sstr << "http://" << ac << ':' << port << std::endl;
		
		// Get the other host names of the machine
		struct hostent *phe = gethostbyname(ac);
		
		// If the machine has other host names
		if (phe != 0)
			// for each host name
			for (int i = 0; phe->h_addr_list[i] != 0; ++i)
			{
				// Get the host name
				struct in_addr addr;
				memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
				
				// Push the host name into the information text
				sstr << "http://" << inet_ntoa(addr) << ':' << port << std::endl;
			}
	}

	// Display the information text that has been pushed into the string stream
	SetWindowText(GetDlgItem(hDlg, IDC_TEXT), sstr.str().c_str());
}
