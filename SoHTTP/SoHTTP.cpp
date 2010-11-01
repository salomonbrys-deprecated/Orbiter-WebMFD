/// \file
/// Salomon Brys HHTP Library Header
/// \author Salomon BRYS <salomon.brys@gmail.com>

/// \addtogroup SoHHTP
/// \{

#include "SoHTTP.h"

#include <iostream>
#include <string>

/// Server main thread (listening loop) start function.
/// Just launck the _loop method on the given SoHTTP object pointer.
DWORD WINAPI startLoop(__in LPVOID lpParameter)
{
	((SoHTTP*)lpParameter)->_loop();
	return 0;
}


/// As handleConnection needs 2 parameters, this structur permit the windows thread function to call handleConnection with 2 parameters.
struct hConnectionParam
{
	SoHTTP* http;
	SOCKET connection;
};

/// Connection thread start function.
/// Just launck the _handleConnection method on the given SoHTTP object pointer with the given socket.
DWORD WINAPI handleConnection(__in LPVOID lpParameter)
{
	hConnectionParam *param = (hConnectionParam*)lpParameter;
	param->http->_handleConnection(param->connection);
	delete param;
	return 0;
}


SoHTTP::SoHTTP()
{
	_childrenThreadsMutex = CreateMutex(NULL, FALSE, NULL);
}_childrenThreads


SoHTTP::~SoHTTP(void)
{
}


bool	SoHTTP::start(int port, int backlog)
{
	// Creating the Socket
	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_socket == INVALID_SOCKET)
	{
		WSACleanup();
		return false;
	}

	// Configuring the socket type
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("0.0.0.0");
	service.sin_port = htons(port);

	// Binding the socket
	if (bind(_socket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
	{
		WSACleanup();
		return false;
	}

	// Setting the socket to listening state
	if (listen(_socket, backlog) == SOCKET_ERROR)
	{
		WSACleanup();
		return false;
	}

	// Creating the main thread (listening loop)
	_thread = CreateThread(NULL, 0, startLoop, (LPVOID*)this, 0, NULL);
	
	// The starting of the server as succeded
	return true;
}


void	SoHTTP::stop()
{
	// Wait to be able to access _childrenThreads by waiting to gain acces to its mutex
	WaitForSingleObject(_childrenThreadsMutex, INFINITE);

	// Terminate all connection threads and sockets
	for (HANDLEList::iterator i = _childrenThreads.begin(); i != _childrenThreads.end(); ++i)
	{
		TerminateThread(i->first, 1);
		closesocket(i->second);
	}
	
	// Emptying _childrenThreads as all connections have been terminated
	_childrenThreads.clear();
	
	// Release the _childrenThreads access mutex
	ReleaseMutex(_childrenThreadsMutex);

	// Terminating the main thread and socket (listening loop)
	TerminateThread(_thread, 1);
	closesocket(_socket);
}


void	SoHTTP::sendFromDir(SOCKET connection, const char * dir, const char * resource)
{
	// Get the resource full path
	std::string path = std::string(dir) + "\\" + resource;
	
	// Getting the attributes of the resource
	DWORD attrs;
	attrs = GetFileAttributes(path.c_str());
	
	// If the resource exists and is a directory, change the resource path and attributes to be those of resource\\index.html
	if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
	{
		path += "\\index.html";
		attrs = GetFileAttributes(path.c_str());
	}
	
	// If the file does not exists, sed a 404 error
	if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		send(connection, "HTTP/1.0 404 NOT FOUND\r\n\r\n", 26, 0);
	else
	{
		// Open the file
		HANDLE file = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		
		// If the file could not be open, send a 404 error
		if (file == INVALID_HANDLE_VALUE)
			send(connection, "HTTP/1.0 404 NOT FOUND\r\n\r\n", 26, 0);
		else
		{
			// Send a 200 OK HTTP code
			send(connection, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
			
			// Send the file (using a 256 bytes buffer)
			char buffer[256];
			DWORD read;
			for (;;)
			{
				if (!ReadFile(file, buffer, 256, &read, NULL))
					break ;
				if (!read)
					break ;
				send(connection, buffer, read, 0);
			}
		}
	}
}


void	SoHTTP::_loop()
{
	// Loop on the listening connection
	for (;;)
	{
		// Wait for a new connection to arrive.
		SOCKET connection = accept(_socket, NULL, NULL);
		
		// If the connection is correct
		if (connection != INVALID_SOCKET)
		{
			// Creating and filling a hConnectionParam to be used by the connection thread start function handleConnection
			hConnectionParam *param = new hConnectionParam;
			param->http = this;
			param->connection = connection;
			
			// Wait to be able to access _childrenThreads by waiting to gain acces to its mutex
			WaitForSingleObject(_childrenThreadsMutex, INFINITE);
			
			// Create the connection thread and register it
			HANDLE thread = CreateThread(NULL, 0, handleConnection, (LPVOID*)param, 0, NULL);
			_childrenThreads.push_back(HSPair(thread, connection));
			
			// Release the _childrenThreads access mutex
			ReleaseMutex(_childrenThreadsMutex);
		}
	}
}


void	SoHTTP::_handleConnection(SOCKET connection)
{
	// The parsing state to know which state is the current in the parsing loop
	enum { PARSING_FISRT_LINE ; PARSING_HEADERS ; READING_BODY } state = PARSING_FISRT_LINE;
	
	// The buffer of character to parse
	std::string buffer;
	
	// The content length possibly provided in the headers, used to read the body
	unsigned int contentLength = 0;

	// The request object to fill
	Request req;

	// PARSING OF HTTP HEADERS
	for (;;)
	{
		// 512 bbytes 0-filled buffer to read from the socket
		char recvBuf[513] = {0, };
		
		// Reading from the socket. Because recvBuf is 0-filled, will essentially creates a 0-terminated string
		// FIXME: Will corrupt the datas on binary bodies
		int len = recv(connection, recvBuf, 512, 0);
		
		// Add what has just been read to the buffer of character to parse 
		buffer += recvBuf;

		// If we are not reading the body, then we are parsing either the first line or the HTTP headers
		if (state != READING_BODY)
		{
			// Position of the next \\n character
			unsigned int pos;
			
			// While there is a line
			while ((pos = buffer.find_first_of("\n")) != std::string::npos)
			{
				// Get this line into the line variable
				std::string line = buffer.substr(0, pos);
				
				// Remove the line from the buffer
				buffer = buffer.substr(pos + 1);
				
				// Remove the ending \\r as HTTP header lines can end by \\r\\n or \\n
				if (line[line.length() - 1] == '\r')
					line = line.substr(0, line.length() - 1);

				// If this is the fisrt line
				if (state == PARSING_FISRT_LINE)
				{
					// Charcter position indicator
					unsigned int i = 0;
					
					// Reading the first word of the first line: the method
					for (; i < line.length() && isalpha(line[i]); ++i)
						req.method += line[i];
					
					// Checking that the line is not finished and that there is a space
					if (i >= line.length() || line[i] != ' ')
					{
						closesocket(_socket);
						return ;
					}
					
					// Ignoring all spaces between the first and the second word
					while (line[i] == ' ')
						++i;

					// Reading the second word of the first line: the resource
					for (; i < line.length() && line[i] != ' '; ++i)
						req.resource += line[i];

					// Checking that the line is not finished and that there is a space
					if (i >= line.length() || line[i] != ' ')
					{
						closesocket(_socket);
						return ;
					}

					// Ignoring all spaces between the first and the second word
					while (line[i] == ' ')
						++i;

					// Reading the third word of the first line: the HTTP Version
					if (line.substr(i, 5) != "HTTP/")
					{
						closesocket(_socket);
						return ;
					}
					i += 5;
					for (; i < line.length() && line[i] != ' '; ++i)
						req.version += line[i];

					// The first line parsing is finished, all the remaining lines are the first the headers then the body
					state = PARSING_HEADERS;
					
					// Ignoring the rest of the line
				}
				// If this is not the first line, this is a header line
				else if (state == PARSING_HEADERS)
				{
					// If this is an empty line, then all the headers have been parsed and the remaining of the stream is the body
					if (line.empty())
						state = READING_BODY;
					// The line is not empty
					else
					{
						// Charcter position indicator
						unsigned int i = 0;
						
						// The name of the header
						std::string headerName;
						
						// Reading all characters before ':' which are the header name
						for (; i < line.length() && line[i] != ':'; ++i)
							headerName += tolower(line[i]);
						
						// Checking that the line is not finished and that there is a colon
						if (i >= line.length() || line[i] != ':')
						{
							closesocket(_socket);
							return ;
						}
						
						// Ignoring the colon
						++i;
						
						// Ignoring all spaces before the header value
						while (line[i] == ' ')
							++i;
						
						// The rest of the line contains the header value
						req.headers[headerName] = line.substr(i);
					}
				}
			}
		}
		
		// If we are reading the body
		// Does not uses an 'else if' because the state may have been changed while the buffer is not empty
		if (state == READING_BODY)
		{
			// Get the content-length if the request has provided one
			if (contentLength == 0 && req.headers.find("content-length") != req.headers.end())
				contentLength = atoi(req.headers["content-length"].c_str());
			
			// Reading the body
			req.body += buffer;
			
			// All the remaining chars in the buffer have been put to the body, so we empty the buffer
			buffer = "";
			
			// If what is read equals or is bigger than the content-length
			// FIXME: Will not read all the body if the content-length is not given, in that case, it should read until connection closes
			if (req.body.length() >= contentLength)
				break ;
		}
	}


	// PARSING OF THE GET STRING

	// The position of the first '?' character in the resource string
	int pos = req.resource.find_first_of('?');
	
	// If there is a '?' in the resource string
	if (pos != std::string::npos)
	{
		// Get the raw get string
		req.getString = req.resource.substr(pos + 1);
		
		// Changing the resource so it now does not include the get string
		req.resource = req.resource.substr(0, pos);
		
		// Copy of the getstring to be modifyable by the parsing algorythm
		std::string getString = req.getString;
		
		// Parsing the get variables...
		do
		{
			// The declaration of only one variable
			std::string str;
			
			// If there is a '&' then isolate the declaration of only one variable
			if ((pos = getString.find_first_of('&')) != std::string::npos)
			{
				str = getString.substr(0, pos);
				getString = getString.substr(pos + 1);
			}
			// The declaration is all that remains
			else
			{
				str = getString;
				getString = "";
			}
			// If there is a '=', then it is indeed a variable declaration, then register it
			if ((pos = str.find_first_of('=')) != std::string::npos)
				req.get[str.substr(0, pos)] = str.substr(pos + 1);
		}
		// ...while there is something to read
		while (!getString.empty());
	}

	// Launching the server-dependant treatement
	// It should write the HTTP response
	handleRequest(connection, req);

	// Wait to be able to access _childrenThreads by waiting to gain acces to its mutex
	WaitForSingleObject(_childrenThreadsMutex, INFINITE);
	
	// Close the socket
	closesocket(connection);
	
	// Unregister the connection thread
	_childrenThreads.remove(HSPair(GetCurrentThread(), connection));
	
	// Release the _childrenThreads access mutex
	ReleaseMutex(_childrenThreadsMutex);
}

/// }