// ==============================================================
//                  ORBITER MODULE: WebMFD
//            Copyright (C) 2010 Salomon BRYS
//                   All rights reserved
//
// Server.h
//
// Class interface for Server. Handles the web server.
// This class is a static singleton.
// ==============================================================

#ifndef __SERVER_H
#define __SERVER_H

#include "SoHTTP/SoHTTP.h"
#include "ServerMFD.h"

#include <map>
#include <queue>
#include <string>

class Server : protected SoHTTP
{
public:
	virtual ~Server(void);

	static Server	&Instance() { return _instance; }

	bool			start(unsigned int port);
	bool			stop();

	bool			isRunning() const { return _isRunning; }
	unsigned int	Port() const { return _port; }

	double			Interval() const { return _interval; }

	ServerMFD		*openMFD(const std::string &key, const std::string &format = "");
	void			closeMFD(const std::string &key, const std::string &format = "" );

	void			clbkOrbiter();

protected:
	virtual void handleRequest(SOCKET connection, Request request);

private:
	void			handleMFDRequest(SOCKET connection, Request request);
	void			handleWebRequest(SOCKET connection, Request request);
	void			handleBtnRequest(SOCKET connection, Request request);
	void			handleBtnHRequest(SOCKET connection, Request request);

	Server();
	static Server		_instance;
	double				_interval;

	bool				_isRunning;
	unsigned int		_port;

	HANDLE				_reqQueue;
	HANDLE				_thread;

	typedef std::map<std::string, ServerMFD*> MFDMap;
	MFDMap				_mfds;
	typedef std::queue<ServerMFD*> MFDQueue;
	MFDQueue			_toRegister;
	MFDQueue			_toUnregister;
	MFDQueue			_forceRefresh;
	typedef std::queue<std::pair<ServerMFD*, int> > BtnQueue;
	BtnQueue			_toPush;

	HANDLE				_mfdsMutex;
};

#endif // __SERVER_H
