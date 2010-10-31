// ==============================================================
//                  ORBITER MODULE: WebMFD
//            Copyright (C) 2010 Salomon BRYS
//                   All rights reserved
//
// Server.h
//
// Implementation of the Web Server. Handles the web server.
// This class is a static singleton.
// ==============================================================

#include "Server.h"
#include "LaunchpadWebMFD.h"

#include <iostream>
#include <sstream>
#include <list>
#include <GdiPlus.h>
#include <Wincrypt.h>
#include <cstring>

typedef std::list<std::string> stringList;

#define ssend(socket, str)	send(socket, str, strlen(str), 0)

extern LaunchpadWebMFD *item;

INT32 getBigEndian(INT32 i);

Server Server::_instance;

Server::Server(void) : _isRunning(false), _port(0), _interval(0.5)
{
	FILEHANDLE ocfg = oapiOpenFile("Orbiter.cfg", FILE_IN);
	oapiReadItem_float(ocfg, "InstrumentUpdateInterval", _interval);
	oapiCloseFile(ocfg, FILE_IN);

	_mfdsMutex = CreateMutex(NULL, FALSE, NULL);
}

Server::~Server(void)
{
	CloseHandle(_mfdsMutex);
}

bool Server::start(unsigned int port)
{
	if (_isRunning)
		return false;

	_port = port;

	SoHTTP::start(_port, 42);

	_isRunning = true;
	return true;
}

bool Server::stop()
{
	if (!_isRunning)
		return false;

	_isRunning = false;

	SoHTTP::stop();

	MFDMap::iterator i = _mfds.begin();
	while (i != _mfds.end())
	{
		MFDMap::iterator c = i;
		++i;

		c->second->unRegister();
		_mfds.erase(c);
	}
	
	return true;
}

ServerMFD * Server::openMFD(const std::string &key, const std::string &format /* = "" */)
{
	WaitForSingleObject(_mfdsMutex, INFINITE);

	if (_mfds.find(key) == _mfds.end())
	{
		if (!format.empty())
		{
			_mfds[key] = new ServerMFD;

			if (format == "png")
				_mfds[key]->addPng();
			else if (format == "jpeg")
				_mfds[key]->addJpeg();
			else
				_mfds[key]->addNox();

			_toRegister.push(_mfds[key]);
		}
		else
		{
			ReleaseMutex(_mfdsMutex);
			return 0;
		}
	}

	ServerMFD *ret = _mfds[key];

	ReleaseMutex(_mfdsMutex);

	return ret;
}

void Server::closeMFD(const std::string &key, const std::string &format /* = "" */)
{
	WaitForSingleObject(_mfdsMutex, INFINITE);

	if (_mfds.find(key) != _mfds.end())
	{
		if (format == "png")
			_mfds[key]->remPng();
		else if (format == "jpeg")
			_mfds[key]->remJpeg();
		else
			_mfds[key]->remNox();

		if (_mfds[key]->Followers() == 0)
		{
			_toUnregister.push(_mfds[key]);
			_mfds.erase(key);
		}
	}

	ReleaseMutex(_mfdsMutex);
}

void Server::clbkOrbiter()
{
	if (WaitForSingleObject(_mfdsMutex, 0) != WAIT_OBJECT_0)
		return ;

	while (!_forceRefresh.empty())
	{
		_forceRefresh.front()->clbkRefreshDisplay(_forceRefresh.front()->GetDisplaySurface());
		_forceRefresh.pop();
	}

	while (!_toRegister.empty())
	{
		_toRegister.front()->Register();
		_toRegister.pop();
	}

	while (!_toUnregister.empty())
	{
		_toUnregister.front()->unRegister();
		_toUnregister.pop();
	}

	while (!_toPush.empty())
	{
		_toPush.front().first->endBtnProcess(_toPush.front().second);
		_toPush.pop();
	}

	ReleaseMutex(_mfdsMutex);
}

void Server::handleRequest(SOCKET connection, Request request)
{
	if (request.resource.substr(0, 5) == "/mfd/")
	{
		request.resource = request.resource.substr(5);
		handleMFDRequest(connection, request);
	}
	else if (request.resource.substr(0, 5) == "/web/")
	{
		request.resource = request.resource.substr(5);
		handleWebRequest(connection, request);
	}
	else if (request.resource.substr(0, 5) == "/btn/")
	{
		request.resource = request.resource.substr(5);
		handleBtnRequest(connection, request);
	}
	else if (request.resource.substr(0, 5) == "/btn_h/")
	{
		request.resource = request.resource.substr(7);
		handleBtnHRequest(connection, request);
	}
	else if (request.resource == "/")
		ssend(connection, "HTTP/1.0 301 Moved Permanently\r\nLocation: /web/\r\n\r\n");
	else
		ssend(connection, "HTTP/1.0 404 Not Found\r\n\r\n");
}

void Server::handleMFDRequest(SOCKET connection, Request request)
{
	if (request.resource.substr(0, 4) == "mfd.")
	{
		std::string format = request.resource.substr(4);

		if (format != "mjpeg" && format != "mpng")
			ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Unkonwn format (only mjpeg and mpng are allowed)</h1>");
		else if (request.get.find("key") == request.get.end())
			ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Need a key</h1>");
		else
		{
			if (format == "mpng")
				format = "png";
			else if (format == "mjpeg")
				format = "jpeg";

			ServerMFD *mfd = openMFD(request.get["key"], format);
			ssend(connection, "HTTP/1.0 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=--NextImage--\r\n");
			unsigned int id = 0;
			for (;;)
			{
				HANDLE file = mfd->getFileIf(format, id);
				if (file != INVALID_HANDLE_VALUE)
				{
					int sent;
					if (mfd->getClose())
					{
						sent = SOCKET_ERROR;
						break ;
					}

					ssend(connection, "\r\n--NextImage--\r\nContent-Type: image/");
					ssend(connection, format.c_str());
					ssend(connection, "\r\n");

					char length[15];
					_itoa_s(GetFileSize(file, NULL), length, 15, 10);
					ssend(connection, "Content-Length: ");
					ssend(connection, length);
					ssend(connection, "\r\n\r\n");

					char buffer[256];
					DWORD read = 1;
					BOOL cont = TRUE;
					while (cont && read)
					{
						cont = ReadFile(file, buffer, 256, &read, NULL);
						if (cont && read)
						{
							sent = send(connection, buffer, read, 0);
							if (sent == SOCKET_ERROR)
								break ;
						}
					}
					if (sent == SOCKET_ERROR)
						break ;
					mfd->closeFile(file);
				}
				Sleep((DWORD)(_interval * 1000 / 20));
			}
			closeMFD(request.get["key"], format);
		}
	}
	else
		ssend(connection, "HTTP/1.0 404 Not Found\r\n\r\n");
}

void Server::handleWebRequest(SOCKET connection, Request request)
{
	if (request.resource == "")
	{
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		hFind = FindFirstFile("WebMFD\\*", &FindFileData);

		if (hFind == INVALID_HANDLE_VALUE)
		{
			printf ("HTTP/1.0 500 Internal Error\r\n\r\n<h1>Could not browse WebMFD directory</h1>", GetLastError());
			return ;
		}
		
		stringList dirs;
		do
			if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY && FindFileData.cFileName[0] != '.')
				dirs.push_back(FindFileData.cFileName);
		while (FindNextFile(hFind, &FindFileData) != 0);
		FindClose(hFind);

		if (dirs.size() == 1)
		{
			ssend(connection, "HTTP/1.0 307 Temporary Redirect\r\nLocation: ");
			ssend(connection, dirs.front().c_str());
			ssend(connection, "/\r\n\r\n");
		}
		else
		{
			ssend(connection, "HTTP/1.0 200 OK\r\n\r\n<html><head><title>Interface list</title></head><body>");
			for (stringList::iterator i = dirs.begin(); i != dirs.end(); ++i)
			{
				ssend(connection, "<p><a href='");
				ssend(connection, i->c_str());
				ssend(connection, "/'>");
				ssend(connection, i->c_str());
				ssend(connection, "</a></p>");
			}
			ssend(connection, "</body></html>");
		}
	}
	else
		sendFromDir(connection, "WebMFD", request.resource.c_str());
}

INT32 decodeSecWebsocket(const std::string & str)
{
	int spaces = 0;
	std::string number;
	for (std::string::const_iterator i = str.begin(); i != str.end(); ++i)
		if (*i == ' ')
			++spaces;
		else if (*i >= '0' && *i <= '9')
			number += *i;
	return (atol(number.c_str()) / spaces);
}

void Server::handleBtnRequest(SOCKET connection, Request request)
{
	if (request.get.find("key") == request.get.end())
	{
		ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Need a key</h1>");
		return ;
	}

	if (	request.headers.find("connection") == request.headers.end()	|| _stricmp(request.headers["connection"].c_str(), "upgrade") != 0
		||	request.headers.find("upgrade") == request.headers.end()	|| _stricmp(request.headers["upgrade"].c_str(), "WebSocket") != 0)
	{
		ssend(connection, "HTTP/1.0 412 Precondition Failed\r\n\r\n<h1>Connection to this URL must use WebSockets</h1>");
		return ;
	}

	if (request.headers.find("host") == request.headers.end() || request.headers.find("origin") == request.headers.end())
	{
		ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Missing host or origin header</h1>");
		return ;
	}

	if (request.headers.find("sec-websocket-key1") == request.headers.end() || request.headers.find("sec-websocket-key2") == request.headers.end())
	{
		ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Missing sec-websocket-key1 or sec-websocket-key2 header</h1>");
		return ;
	}

	ServerMFD *mfd = 0;
	
	for (unsigned int tries = 0; tries < 42; ++tries)
	{
		mfd = openMFD(request.get["key"]);
		if (mfd)
			break ;
		Sleep(450);
	}

	if (!mfd)
	{
		ssend(connection, "HTTP/1.0 412 Precondition Failed\r\n\r\n<h1>Could not find the MFD</h1>");
		return ;
	}

	union
	{
		struct
		{
			INT32 sec1;
			INT32 sec2;
			char key[8];
		} head;
		BYTE ascii[16];
	} handshake;

	handshake.head.sec1 = getBigEndian(decodeSecWebsocket(request.headers["sec-websocket-key1"]));
	handshake.head.sec2 = getBigEndian(decodeSecWebsocket(request.headers["sec-websocket-key2"]));

	while (request.body.length() < 8)
	{
		char tmp[8];
		int r = recv(connection, tmp, 8, 0);
		request.body += std::string(tmp, r);
	}
	strncpy(handshake.head.key, request.body.substr(0, 8).c_str(), 8);

	HCRYPTPROV hProv = 0;
	CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);

	HCRYPTHASH hHash = 0;
	CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);

	CryptHashData(hHash, handshake.ascii, 16, 0);

	DWORD cbHash = 16; // = MD5 Len
	BYTE rgbHash[16];
	CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);

	CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

	ssend(connection, "HTTP/1.1 101 WebSocket Protocol Handshake\r\n");
	ssend(connection, "Connection: Upgrade\r\n");
	ssend(connection, "Upgrade: WebSocket\r\n");
	ssend(connection, "Sec-WebSocket-Origin: ");
		ssend(connection, request.headers["origin"].c_str());
		ssend(connection, "\r\n");
	ssend(connection, "Sec-WebSocket-Location: ws://");
		ssend(connection, request.headers["host"].c_str());
		ssend(connection, "/btn/?");
		ssend(connection, request.getString.c_str());
		ssend(connection, "\r\n");
	ssend(connection, "Sec-WebSocket-Protocol: Orbiter-WebMFD-Buttons\r\n");
	ssend(connection, "\r\n");

	send(connection, (char*)rgbHash, 16, 0);

	for (;;)
	{
		unsigned char buf[5] = {0,};
		int ret = recv(connection, (char*)buf, 4, 0);
		if (ret <= 0)
			break ;

		int btnId = atoi((const char *)buf + 1);

		if (btnId >= 0)
		{
			mfd->startBtnProcess();
			WaitForSingleObject(_mfdsMutex, INFINITE);
			_toPush.push(std::pair<ServerMFD*, int>(mfd, btnId));
			ReleaseMutex(_mfdsMutex);
			mfd->waitForBtnProcess();
			if (btnId == 99)
				break ;
			else
				_forceRefresh.push(mfd);
		}

		send(connection, "\0", 1, 0);
		std::string JSON = mfd->getJSON();
		ssend(connection, JSON.c_str());
		int sent = send(connection, "\xFF", 1, 0);
		if (sent == SOCKET_ERROR)
			break ;
	}

	closeMFD(request.get["key"]);
}

void Server::handleBtnHRequest(SOCKET connection, Request request)
{
	if (request.get.find("key") == request.get.end())
	{
		ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Need a key</h1>");
		return ;
	}

	ServerMFD *mfd = openMFD(request.get["key"]);

	if (!mfd)
	{
		ssend(connection, "HTTP/1.0 412 Precondition Failed\r\n\r\n<h1>Could not find the MFD</h1>");
		return ;
	}

	if (!request.resource.empty())
	{
		int btnId = atoi(request.resource.c_str());
		if (btnId >= 0)
		{
			mfd->startBtnProcess();
			WaitForSingleObject(_mfdsMutex, INFINITE);
			_toPush.push(std::pair<ServerMFD*, int>(mfd, btnId));
			ReleaseMutex(_mfdsMutex);
			mfd->waitForBtnProcess();
			if (btnId != 99)
				_forceRefresh.push(mfd);
		}
	}

	ssend(connection, mfd->getJSON().c_str());
	closeMFD(request.get["key"]);
}
