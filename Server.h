/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#ifndef __SERVER_H
#define __SERVER_H

#include "SoHTTP/SoHTTP.h"
#include "ServerMFD.h"

#include <map>
#include <queue>
#include <string>

/// Handles the web server using SoHTTP.
/// This class is a static singleton : There can be only one running server for a simulation.
class Server : protected SoHTTP
{
public:
	/// Virtual destructor
	virtual ~Server();

	/// Singleton method that gets the running instance
	/// \return the running singleton instance
	static Server	&Instance() { return _instance; }


	/// Start the server on the given port
	/// \param[in]	port	The port on which to start the server
	/// \return wether the starting has succeded or not
	bool			start(unsigned int port);
	
	/// Stop the running server
	/// \return wether the stoping has succeded or not
	bool			stop();

	
	/// Informs wether the server is running or not
	/// \return Wether the server is running or not
	bool			isRunning() const { return _isRunning; }
	
	/// Gets the port on which the server is running
	/// \return the port on which the server is running
	unsigned int	Port() const { return _port; }

	/// Gets the interval of MFD refresh configured in orbiter
	/// \return the interval of MFD refresh
	double			Interval() const { return _interval; }

	/// Opens a MFD.
	///  - Creates a MFD if a MFD does not already exists for the given key.
	///  - Return the corresponding MFD if it already has been created for the given key.
	/// The closeMFD MUST be called when the usage of the returned MFD is finished.
	/// \param[in]	key		A key on which to register the MFD
	/// \param[in]	format	"mpng" or "mjpeg" or "". If no null, used to inform the MFD refresh thread that there will be at least one thread that will need image from this format.
	/// \return An existing or newly created ServerMFD pointer
	ServerMFD		*openMFD(const std::string &key, const std::string &format = "");
	
	/// Closes a MFDMap
	/// Must be called with the exact same parameter as given to OpenMFD.
	/// \param[in]	key		The key on which the opened MFD was registered.
	/// \param[in]	format	"mpng" or "mjpeg" or "". The image format on which the MFD was informed.
	void			closeMFD(const std::string &key, const std::string &format = "" );

	/// Callback used by WebMFD.cpp to update the open and registered MFDs.
	void			clbkOrbiter();

protected:
	/// Callback used by SoHTTP to handle a HTTP request / connection.
	virtual void handleRequest(SOCKET connection, Request & request);

private:
	/// Treatment function called when a MFD is requested.
	/// Handles the motion image stream.
	/// \param[in]	connection	The connection of the request
	/// \param[in]	request		The requestion information structure
	void			handleMFDRequest(SOCKET connection, Request & request);

	/// Treatment function called when a classic file is requested.
	/// Handles the diferent files of the diferent installed web-interfaces.
	/// If there is only one web-interface, it will automatically redirect to it.
	/// \param[in]	connection	The connection of the request
	/// \param[in]	request		The requestion information structure
	void			handleWebRequest(SOCKET connection, Request & request);

	/// Treatment function called when a button WebSocket connection is requested
	/// Handles Websocket input and output
	/// \param[in]	connection	The connection of the request
	/// \param[in]	request		The requestion information structure
	void			handleBtnRequest(SOCKET connection, Request & request);

	/// Treatment function called when a Button regular enquiry is requested
	/// \param[in]	connection	The connection of the request
	/// \param[in]	request		The requestion information structure
	void			handleBtnHRequest(SOCKET connection, Request & request);

	/// Private constructor (as needed for a singleton)
	Server();
	
	/// The singleton instance
	static Server		_instance;
	
	/// The MFD refresh interval as read in the orbiter configuration
	double				_interval;

	
	/// Wether the server is currently running or not
	bool				_isRunning;
	
	/// The port on which the server runs
	unsigned int		_port;

	
	typedef std::map<std::string, ServerMFD*> MFDMap;

	/// The map of MFD pointers, each corresponding to a key
	MFDMap				_mfds;

	typedef std::queue<ServerMFD*> MFDQueue;
	
	/// The newly created MFDs to be registered in Orbiter in the main thread
	MFDQueue			_toRegister;
	
	/// The MFDs to be unregistered in Orbiter in the main thread
	MFDQueue			_toUnregister;
	
	/// The MFDs to force refreshment in Orbiter in the main thread
	MFDQueue			_forceRefresh;

	typedef std::queue<std::pair<ServerMFD*, int> > BtnQueue;
	
	/// The button to inform pression in Orbiter in the main thread
	BtnQueue			_toPush;

	/// The mutex to access MFD Map and different event queues
	HANDLE				_mfdsMutex;
};

#endif // __SERVER_H
