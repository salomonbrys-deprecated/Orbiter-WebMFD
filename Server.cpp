/// \file
/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Copyright (C) 2006 Martin Schweiger
/// Copyright (C) 2010 File authors
/// License LGPL

#include "Server.h"
#include "LaunchpadWebMFD.h"

#include <iostream>
#include <list>
#include <GdiPlus.h>
#include <Wincrypt.h>
#include <cstring>

typedef std::list<std::string> stringList;

/// Utility macro to send a null-terminated charcater string on a socket.
#define ssend(socket, str)	send(socket, str, strlen(str), 0)

/// Macro that defines an int that is the MFD refresh time divided by 10 in milliseconds if it is > 10, else 10
#define WEBMFD_REFRESH_ASK_MS (((_interval * 1000 / 10) > 10) ? (_interval * 1000 / 10) : 10)

INT32 getBigEndian(INT32 i);

Server Server::_instance;

Server::Server(void) : _isRunning(false), _port(0), _interval(0.5)
{
	// Get the MFD refresh interval from the Orbiter configuration
	FILEHANDLE ocfg = oapiOpenFile("Orbiter.cfg", FILE_IN);
	oapiReadItem_float(ocfg, "InstrumentUpdateInterval", _interval);
	oapiCloseFile(ocfg, FILE_IN);

	// Creates the mutex to access the shared resources
	_mfdsMutex = CreateMutex(NULL, FALSE, NULL);
}


Server::~Server(void)
{
	// Delete the mutex to access the shared resources
	CloseHandle(_mfdsMutex);
}


bool Server::start(unsigned int port)
{
	// Do nothing if the server is already running
	if (_isRunning)
		return false;

	// Set the port from the argument
	_port = port;

	// Start the server (using SoHTTP)
	_isRunning = SoHTTP::start(_port, 42);

	// return wether the starting has succeded
	return _isRunning;
}


bool Server::stop()
{
	// Do nothing if the server is not running
	if (!_isRunning)
		return false;

	// Stop the server (using SoHTTP)
	SoHTTP::stop();

	// Set that the server is not running anymore
	_isRunning = false;

	// Wait to be able to access _mfds by waiting to gain acces to its mutex
	WaitForSingleObject(_mfdsMutex, INFINITE);

	// Unregister all registered MFDs
	for (MFDMap::iterator i = _mfds.begin(); i != _mfds.end(); ++i)
		i->second->unRegister();

	// Clear the map
	_mfds.clear();
	
	// Release the _mfds access mutex
	ReleaseMutex(_mfdsMutex);

	// Stoping has succeeded
	return true;
}


ServerMFD * Server::openMFD(const std::string &key, const std::string &format /* = "" */, bool create /* = true */)
{
	// Wait to be able to access _mfds by waiting to gain acces to its mutex
	WaitForSingleObject(_mfdsMutex, INFINITE);

	// If the mutex does not exists
	if (_mfds.find(key) == _mfds.end())
	{
		// Return null pointer if not asked to create a MFD
		if (!create)
		{
			ReleaseMutex(_mfdsMutex);
			return 0;
		}

		// Create a new MFD
		_mfds[key] = new ServerMFD;

		// Register the given format
		if (format == "png")
			_mfds[key]->addPng();
		else if (format == "jpeg")
			_mfds[key]->addJpeg();
		else
			_mfds[key]->addNox();

		// Add the MFD to Orbiter registration queue
		// We are probably not in the main (Orbiter) thread
		// Therefore, we cannot use directly the Orbiter API as Orbiter is not thread-safe (Martin Schweiger if you read this...)
		_toRegister.push(_mfds[key]);
	}

	// The pointer to return.
	// Need a temporary pointer as we cannot access _mfds after releasing its mutex
	ServerMFD *retTMP = _mfds[key];

	// Release the _mfds access mutex
	ReleaseMutex(_mfdsMutex);

	// Return the MFD
	return retTMP;
}


void Server::closeMFD(const std::string &key, const std::string &format /* = "" */)
{
	// Wait to be able to access _mfds by waiting to gain acces to its mutex
	WaitForSingleObject(_mfdsMutex, INFINITE);

	// If the given key references a registered MFD
	if (_mfds.find(key) != _mfds.end())
	{
		
		// Unregister the given format
		if (format == "png")
			_mfds[key]->remPng();
		else if (format == "jpeg")
			_mfds[key]->remJpeg();
		else
			_mfds[key]->remNox();

		// If the MFD has no "followers" any more
		if (_mfds[key]->Followers() == 0)
		{
			// Add the MFD to Orbiter unregistration queue
			// We are probably not in the main (Orbiter) thread
			// Therefore, we cannot use directly the Orbiter API as Orbiter is not thread-safe (Martin Schweiger if you read this...)
			_toUnregister.push(_mfds[key]);
			
			// Remove the MFD from the registered MFDs map
			_mfds.erase(key);
		}
	}

	// Release the _mfds access mutex
	ReleaseMutex(_mfdsMutex);
}


void Server::clbkOrbiterPreStep()
{
	// Try to be able to access _mfds by trying to gain acces to its mutex
	// As this callback will be called *every* frame, we do not wait for the mutex to be free.
	// We just do nothing and pospone the treatment to the next frame, the mutex will probably get released by then.
	if (WaitForSingleObject(_mfdsMutex, 0) != WAIT_OBJECT_0)
		return ;

	// Call the Orbiter MFD clbkRefreshDisplay for each refresh event registered MFD.
	for (; !_forceRefresh.empty(); _forceRefresh.pop())
		_forceRefresh.front()->clbkRefreshDisplay(_forceRefresh.front()->GetDisplaySurface());

	// Call the Orbiter MFD Register for each register event registered MFD.
	for (; !_toRegister.empty(); _toRegister.pop())
		_toRegister.front()->Register();

	// Call the Orbiter MFD unRegister for each unregister event registered MFD.
	for (; !_toUnregister.empty(); _toUnregister.pop())
		_toUnregister.front()->unRegister();

	// Call the Orbiter MFD endBtnProcess for each registered MFD.
	for (; !_btnEnd.empty(); _btnEnd.pop())
		_btnEnd.front()->endBtnProcess();

	// Call the Orbiter MFD execBtnProcess for each button pressed event registered MFD.
	for (; !_btnPress.empty(); _btnPress.pop())
	{
		_btnPress.front().first->execBtnProcess(_btnPress.front().second);
		
		// Register the end of the button press process *after* that orbiter has processed it.
		_btnEnd.push(_btnPress.front().first);
	}

	// Release the _mfds access mutex
	ReleaseMutex(_mfdsMutex);
}


void Server::handleRequest(SOCKET connection, Request & request)
{
	// If the request is for a MFD
	if (request.resource.substr(0, 5) == "/mfd/")
	{
		// Remove the "/mfd/" from the resource string
		request.resource = request.resource.substr(5);
		
		// Handle the request
		handleMFDRequest(connection, request);
	}
	
	// If the request is for a file
	else if (request.resource.substr(0, 5) == "/web/")
	{
		// Remove the "/web/" from the resource string
		request.resource = request.resource.substr(5);
		
		// Handle the request
		handleWebRequest(connection, request);
	}
	
	// If the request is for a button WebSocket
	else if (request.resource.substr(0, 5) == "/btn/")
	{
		// Remove the "/btn/" from the resource string
		request.resource = request.resource.substr(5);
		
		// Handle the request
		handleBtnRequest(connection, request);
	}
	
	// If the request is for a button classic HTTP enquiry
	else if (request.resource.substr(0, 7) == "/btn_h/")
	{
		// Remove the "/btn_h/" from the resource string
		request.resource = request.resource.substr(7);
		
		// Handle the request
		handleBtnHRequest(connection, request);
	}
	
	// If the request is for root, redirect to /web/
	else if (request.resource == "/")
		ssend(connection, "HTTP/1.0 301 Moved Permanently\r\nLocation: /web/\r\n\r\n");
	
	// The request is unknown: send a 404 error
	else
		ssend(connection, "HTTP/1.0 404 Not Found\r\n\r\n");
}


void Server::handleMFDRequest(SOCKET connection, Request & request)
{
	// If a MFD request has to be /mfd/mfd.[format]
	// If the resource asked starts with "mfd."
	// Which means that it is probably a correct request.
	if (request.resource.substr(0, 4) == "mfd.")
	{
		// Remove the "mfd." from the resource string
		std::string format = request.resource.substr(4);

		// If the format of the resource (the remaining string) is not "mpng" or "mjpeg", send a 400 error
		if (format != "mjpeg" && format != "mpng")
			ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Unkonwn format (only mjpeg and mpng are allowed)</h1>");
		
		// If the key is not given in the request in get variable, send a 400 error
		else if (request.get.find("key") == request.get.end())
			ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Need a key</h1>");
		
		// The request is correct
		else
		{
			// The number of time we have waited for a tenth of the refreshing time.
			int nbWaits = 0;

			// Remove the 'm' of "mjpeg" or "mpng"
			format = format.substr(1);

			// Get the MFD for the corresponding key
			ServerMFD *mfd = openMFD(request.get["key"], format);

			// Check that the MFD has correctly been opened
			if (!mfd)
				return ;
			
			// Send a 200 HTTP code followed by the Content-Type header needed for the motion image stream
			ssend(connection, "HTTP/1.0 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=--MFDNextImage--\r\n");

			// Id of the image, used to check if the image has changed
			unsigned int id = 0;
			
			// Infinite loop that will run until the connection stops
			for (;;)
			{
				// Get a stream containing the image if, and only if, the id of the image has changed
				// Will also update the id to the id of the new image
				imageStream * stream = mfd->getStreamIf(format, id);
				
				// If there is a new image
				if (stream)
				{
					// If the close button has been pressed, break the stream
					if (mfd->getClose())
						break ;

					// Send the next image boundary in the motion image stream
					ssend(connection, "\r\n--MFDNextImage--\r\nContent-Type: image/");
					ssend(connection, format.c_str());
					ssend(connection, "\r\n");

					// Send the content-length header and the empty line indicating the end of the headers in the motion image stream
					char length[15];
					_itoa_s(stream->size, length, 15, 10);
					ssend(connection, "Content-Length: ");
					ssend(connection, length);
					ssend(connection, "\r\n\r\n");

					// The buffer used to read the image
					char buffer[256];
					
					// The number of bytes read at each iteration of the read loop
					// Set to one to enable to start the loop
					ULONG read;
					
					// The Read call return status
					// Wether Read succeeds
					HRESULT readHr;
					
					// Number of byte read at each iteration of the read loop
					int sent;
					
					// The total number of bytes sent
					int totalSent = 0;

					// Transfer from stream to socket...
					do
					{
						// The number of bytes to read left in the image
						int bytesToRead = stream->size - totalSent;

						// Read by range of maximum 256 bytes
						if (bytesToRead > 256)
							bytesToRead = 256;

						// Read the bytes of the stream
						readHr = stream->stream->Read(buffer, bytesToRead, &read);
						
						// If the read has succeeded
						if (readHr == S_OK && read)
						{
							// Write on the socket what has been read from the stream
							sent = send(connection, buffer, read, 0);
							totalSent += sent;
						}
					}
					// ...While the read has succeded and more bytes are to be read
					while (readHr == S_OK && totalSent < stream->size && read && sent != SOCKET_ERROR);

					// If the last socket send has failed, then it means that the socket has been close, break the stream
					if (sent == SOCKET_ERROR)
						break ;
					
					// Release the image stream
					mfd->closeStream();

					// reset the number of waits
					nbWaits = 0;
				}
				else
				{
					// Increment the number of times we have waited since the last image
					++nbWaits;

					// If we have waited more then a refreshing time
					if (nbWaits > 10)
					{
						// Force the MFD refresh
						WaitForSingleObject(_mfdsMutex, INFINITE);
						_forceRefresh.push(mfd);
						ReleaseMutex(_mfdsMutex);

						// reset the number of waits
						nbWaits = 0;
					}
				}
				// Wait for a tenth of the refreshing interval before asking if there is a new image
				Sleep((DWORD)(WEBMFD_REFRESH_ASK_MS));
			}
			
			// Close the MFD
			closeMFD(request.get["key"], format);
		}
	}
	// The resource does not starts with "mfd." and is threfore an incorect request: send a 404 error
	else
		ssend(connection, "HTTP/1.0 404 Not Found\r\n\r\n");
}


void Server::handleWebRequest(SOCKET connection, Request & request)
{
	// If the file request does not specify a resource
	if (request.resource == "")
	{
		// Open an item list of the WebMFD directory
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		hFind = FindFirstFile("WebMFD\\*", &FindFileData);

		// If the WebMFD directory cannot be listed, send a 500 error and return
		if (hFind == INVALID_HANDLE_VALUE)
		{
			printf ("HTTP/1.0 500 Internal Error\r\n\r\n<h1>Could not browse WebMFD directory</h1>", GetLastError());
			return ;
		}
		
		// The list in which all the WebMFD sub-directories names will be stored
		stringList dirs;
		
		// Browse the WebMFD directory...
		do
			// If the item is a directory and is not the current directory
			if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY && FindFileData.cFileName[0] != '.')
				// Add it's name to the list
				dirs.push_back(FindFileData.cFileName);
		// ...While there are item to be found
		while (FindNextFile(hFind, &FindFileData) != 0);
		
		// close the item list
		FindClose(hFind);

		// If only one subdirectoy has been found, redirect to this subdirectory
		if (dirs.size() == 1)
		{
			ssend(connection, "HTTP/1.0 307 Temporary Redirect\r\nLocation: ");
			ssend(connection, dirs.front().c_str());
			ssend(connection, "/\r\n\r\n");
		}
		// There are more than one sub-directory
		else
		{
			// Send a 200 HTTP code along with the HTML header of the list
			ssend(connection,
				"HTTP/1.0 200 OK\r\n"
				"\r\n"
				"<html>"
					"<head>"
						"<title>Interface list</title>"
						"<link rel='Stylesheet' type='text/css' href='_InterfaceChoice.css' />"
					"</head>"
					"<body>"
						"<h1>Choose an Interface</h1>"
			);
			
			// For each sub-directory name in the list
			for (stringList::iterator i = dirs.begin(); i != dirs.end(); ++i)
			{
				// Send a HTML hyperlink for it
				ssend(connection, "<div class='interface'><h3><a href='");
				ssend(connection, i->c_str());
				ssend(connection, "/'>");
				ssend(connection, i->c_str());
				ssend(connection, "</a></h3>");

				// If there is a description
				HANDLE descFile = CreateFile((std::string("WebMFD\\") + *i + "\\description.txt").c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (descFile != INVALID_HANDLE_VALUE)
				{
					// Send the description (using a 256 bytes buffer)
					ssend(connection, " <p class='desc'>");
					char buffer[256];
					DWORD read;
					for (;;)
					{
						if (!ReadFile(descFile, buffer, 256, &read, NULL))
							break ;
						if (!read)
							break ;
						send(connection, buffer, read, 0);
					}
					ssend(connection, "</p>");
				}
				ssend(connection, "</div>");
			}
			
			// Send the HTML footer of the list
			ssend(connection,
						"<p class='footer'>Tip: if you are always using the same interface, delete all the others in the WebMFD directory to automatically use the remain one</p>"
					"</body>"
				"</html>"
			);
		}
	}
	// The request does specifies a resource: the resource is a file
	else
		// Use the SoHTTP utility function to send the requested file
		sendFromDir(connection, "WebMFD", request.resource.c_str());
}


/// Utility function for web socket: decodes a Sec-WebSocket-Key and calculate the needed result value for the handshake
/// The Sec-WebSocket-Key calculation is explained here: http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-03#section-1.3
/// \note I don't know what the hell happend to the guy that made this protocol, but I want what he smoked !
/// \param[in]	str	The Sec-Header value
/// \return the needed handshake
INT32 decodeSecWebsocket(const std::string & str)
{
	// Variable used to count the numbers of spaces in the string
	int spaces = 0;

	// Used to get a number obtained by concatenate all digits in the string
	std::string number;

	// For each charcater in the value string
	for (std::string::const_iterator i = str.begin(); i != str.end(); ++i)
		// If the character is a space, increment the space count vraiable
		if (*i == ' ')
			++spaces;
		// If the character is a digit, append it to the number string
		else if (*i >= '0' && *i <= '9')
			number += *i;

	// return the number obtained by the digit concatenation divided by the number of spaces
	return (atol(number.c_str()) / spaces);
}


void Server::handleBtnRequest(SOCKET connection, Request & request)
{
	// The button request must have a get variable name 'key'. If not, send a 400 error and return
	if (request.get.find("key") == request.get.end())
	{
		ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Need a key</h1>");
		return ;
	}

	// The request must be a web-socket request, check that the request provided the needed WebSocket information headers
	// If not, send a 412 error and return
	if (	request.headers.find("connection") == request.headers.end()	|| _stricmp(request.headers["connection"].c_str(), "upgrade") != 0
		||	request.headers.find("upgrade") == request.headers.end()	|| _stricmp(request.headers["upgrade"].c_str(), "WebSocket") != 0)
	{
		ssend(connection, "HTTP/1.0 412 Precondition Failed\r\n\r\n<h1>Connection to this URL must use WebSockets</h1>");
		return ;
	}

	// The request must have a host and a origin headers. If not, send a 400 error and return
	if (request.headers.find("host") == request.headers.end() || request.headers.find("origin") == request.headers.end())
	{
		ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Missing host or origin header</h1>");
		return ;
	}

	// The request must have a sec-websocket-key 1 and 2 headers. If not, send a 400 error and return
	if (request.headers.find("sec-websocket-key1") == request.headers.end() || request.headers.find("sec-websocket-key2") == request.headers.end())
	{
		ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Missing sec-websocket-key1 or sec-websocket-key2 header</h1>");
		return ;
	}

	// The MFD to use the buttons on
	ServerMFD *mfd = 0;
	
	// Open the MFD (with no image format)
	mfd = openMFD(request.get["key"]);

	// If the open of the MFD failed, send a 500 error and return
	if (!mfd)
	{
		ssend(connection, "HTTP/1.0 500 Internal Server Error\r\n\r\n<h1>Could not open the MFD</h1>");
		return ;
	}

	// Used to calculate the response security string of the WebSocket protocol
	// C.f. decodeSecWebsocket note...
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

	// Getting the sec-websocket-key2 calculation results
	handshake.head.sec1 = getBigEndian(decodeSecWebsocket(request.headers["sec-websocket-key1"]));
	handshake.head.sec2 = getBigEndian(decodeSecWebsocket(request.headers["sec-websocket-key2"]));

	// Getting the 8 bytes of the body
	// As soHTTP does not ensure that all the body is read,
	// read from the socket and add the result to the body
	// while the body does not contains the 8 needed bytes
	while (request.body.length() < 8)
	{
		char tmp[8];
		int r = recv(connection, tmp, 8, 0);
		request.body += std::string(tmp, r);
	}
	
	// Copying the body 8 bytes into the WebSocket security structure
	strncpy(handshake.head.key, request.body.substr(0, 8).c_str(), 8);

	// Getting a windows cryptography context
	HCRYPTPROV hProv = 0;
	CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);

	// Getting a windows MD5 Hash object from the cryptography context
	HCRYPTHASH hHash = 0;
	CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);

	// Hashing the security handshake using the MD5 Hash object
	CryptHashData(hHash, handshake.ascii, 16, 0);

	// Getting the resultof the MD5 Hash into rgbHash
	DWORD cbHash = 16; // = MD5 Len
	BYTE rgbHash[16];
	CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);

	// Destroying the Hash object and cryptographic context
	CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

	// Sending the WebSocket headers
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

	// Sending the WebSocket hashed handshake
	send(connection, (char*)rgbHash, 16, 0);

	// Set the connection socket to timeout.
	// This is necessary for regular button check
	int toVal = (int)WEBMFD_REFRESH_ASK_MS;
	setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (char*)&toVal, sizeof(int));

	// Id of the button labels, used to check if the button labels have changed
	unsigned int BtnPrevId = 0;

	// Infinite loop that will run until the connection stops
	for (;;)
	{
		// Get the JSON string if the button labels have changed
		std::string JSON = mfd->getJSONIf(BtnPrevId);

		// If the JSON string has evolved, it is returned
		if (!JSON.empty())
		{
			// send the buttons status in JSON inside a WebMFD message
			send(connection, "\0", 1, 0);
			ssend(connection, JSON.c_str());
			int sent = send(connection, "\xFF", 1, 0);

			// If the sending of the response has failed, break the connection
			if (sent == SOCKET_ERROR)
				break ;
		}

		// The buffer to read the buttons pressed
		unsigned char buf[5] = {0,};
		
		// Read 4 bytes from the sockets which should be:
		//   255 <btnId-ascii-digit-tenth> <btnId-ascii-digit-unit> 0
		int ret = recv(connection, (char*)buf, 4, 0);

		// If the read has failed
		if (ret <= 0)
		{
			// If the read has failed because of a timeout, continue the loop
			if (WSAGetLastError() == WSAETIMEDOUT)
				continue ;
			
			// If the read has failed for any other reason, break the connection
			break;
		}

		// If the packet is not a 4 bytes websocket message, ignores it
		if (ret != 4 || buf[0] != 0 || buf[3] != 255)
			continue ;

		// Reading the btnId (ignoring the first byte, which is 255)
		int btnId = atoi((const char *)buf + 1);

		// If the given button Id is a correct number
		if (btnId >= 0)
		{
			// Inform that we want to start a button press
			mfd->startBtnProcess();
			
			// Register the button press into the corresponding event queue
			// We are probably not in the main (Orbiter) thread
			// Therefore, we cannot use directly the Orbiter API as Orbiter is not thread-safe (Martin Schweiger if you read this...)
			WaitForSingleObject(_mfdsMutex, INFINITE);
			_btnPress.push(std::pair<ServerMFD*, int>(mfd, btnId));
			ReleaseMutex(_mfdsMutex);
			
			// Wait for the end of the button process
			mfd->waitForBtnProcess();
			
			// If the button pressed was the close one, break the connection
			if (btnId == 99)
				break ;

			// Force the refresh of the MFD as the button press has probably a consequence on the display
			WaitForSingleObject(_mfdsMutex, INFINITE);
			_forceRefresh.push(mfd);
			ReleaseMutex(_mfdsMutex);
		}
		
		// If the given button id is -1, it means that this is just an enquiry and not a button press
		// Therefore, BtnPrevId is set to 0, which will force, in the begining of the next loop, the button JSON send
		else if (btnId == -1)
			BtnPrevId = 0;
	}

	// Close the MFD
	closeMFD(request.get["key"]);
}

void Server::handleBtnHRequest(SOCKET connection, Request & request)
{
	// The button request must have a get variable name 'key'. If not, send a 400 error and return
	if (request.get.find("key") == request.get.end())
	{
		ssend(connection, "HTTP/1.0 400 BAD REQUEST\r\n\r\n<h1>Need a key</h1>");
		return ;
	}

	// Open the MFD (with no image format)
	ServerMFD *mfd = openMFD(request.get["key"], "", false);

	// If the open of the MFD failed, send a 500 error and return
	if (!mfd)
	{
		ssend(connection, "HTTP/1.0 412 Precondition Failed\r\n\r\n<h1>Could not find the MFD</h1>");
		return ;
	}

	// Send a 200 HTTP error code
	ssend(connection, "HTTP/1.0 200 OK\r\n\r\n");

	// Get the button id from the resource
	int btnId = atoi(request.resource.c_str());
	
	// If the button Id is a correct number
	// It may very well be -1, which means that this is just an enquiry and not a button press
	if (btnId >= 0)
	{
		// Inform that we want to start a button press
		mfd->startBtnProcess();

		// Register the button press into the corresponding event queue
		// We are probably not in the main (Orbiter) thread
		// Therefore, we cannot use directly the Orbiter API as Orbiter is not thread-safe (Martin Schweiger if you read this...)
		WaitForSingleObject(_mfdsMutex, INFINITE);
		_btnPress.push(std::pair<ServerMFD*, int>(mfd, btnId));
		ReleaseMutex(_mfdsMutex);

		// Wait for the end of the button process
		mfd->waitForBtnProcess();
		
		// Force the refresh of the MFD as the button press has probably a consequence on the display
		WaitForSingleObject(_mfdsMutex, INFINITE);
		_forceRefresh.push(mfd);
		ReleaseMutex(_mfdsMutex);
	}

	// send the buttons status in JSON
	unsigned int btnPrevId = 0;
	ssend(connection, mfd->getJSON().c_str());

	// Close the MFD
	closeMFD(request.get["key"]);
}
