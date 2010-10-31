#include "SoHTTP.h"

#include <iostream>
#include <string>

DWORD WINAPI startLoop(__in LPVOID lpParameter)
{
	((SoHTTP*)lpParameter)->_loop();
	return 0;
}


struct hConnectionParam
{
	SoHTTP* http;
	SOCKET connection;
};


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
}


SoHTTP::~SoHTTP(void)
{
}


bool	SoHTTP::start(int port, int backlog)
{
	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_socket == INVALID_SOCKET)
	{
		WSACleanup();
		return false;
	}

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("0.0.0.0");
	service.sin_port = htons(port);

	if (bind(_socket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
	{
		WSACleanup();
		return false;
	}

	if (listen(_socket, backlog) == SOCKET_ERROR)
	{
		WSACleanup();
		return false;
	}

	_thread = CreateThread(NULL, 0, startLoop, (LPVOID*)this, 0, NULL);
	return true;
}


void	SoHTTP::stop()
{
	WaitForSingleObject(_childrenThreadsMutex, INFINITE);
	for (HANDLEList::iterator i = _childrenThreads.begin(); i != _childrenThreads.end(); ++i)
	{
		TerminateThread(i->first, 1);
		closesocket(i->second);
	}
	_childrenThreads.clear();
	ReleaseMutex(_childrenThreadsMutex);

	TerminateThread(_thread, 1);
	closesocket(_socket);
}


void	SoHTTP::sendFromDir(SOCKET connection, const char * dir, const char * resource)
{
	std::string path = std::string(dir) + "\\" + resource;
	DWORD attrs;
	attrs = GetFileAttributes(path.c_str());
	if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
	{
		path += "\\index.html";
		attrs = GetFileAttributes(path.c_str());
	}
	if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		send(connection, "HTTP/1.0 404 NOT FOUND\r\n\r\n", 26, 0);
	else
	{
		HANDLE file = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
			send(connection, "HTTP/1.0 404 NOT FOUND\r\n\r\n", 26, 0);
		else
		{
			send(connection, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
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
	for (;;)
	{
		SOCKET connection = accept(_socket, NULL, NULL);
		if (connection != INVALID_SOCKET)
		{
			hConnectionParam *param = new hConnectionParam;
			param->http = this;
			param->connection = connection;
			WaitForSingleObject(_childrenThreadsMutex, INFINITE);
			HANDLE thread = CreateThread(NULL, 0, handleConnection, (LPVOID*)param, 0, NULL);
			_childrenThreads.push_back(HSPair(thread, connection));
			ReleaseMutex(_childrenThreadsMutex);
		}
	}
}


void	SoHTTP::_handleConnection(SOCKET connection)
{
	unsigned short int state = 0;
	std::string buffer;
	unsigned int contentLength = 0;

	Request req;

	// PARSING OF HTTP HEADERS
	for (;;)
	{
		char recvBuf[513] = {0, };
		int len = recv(connection, recvBuf, 512, 0);
		buffer += recvBuf;

		if (state < 2)
		{
			unsigned int pos;
			while ((pos = buffer.find_first_of("\n")) != std::string::npos)
			{
				std::string line = buffer.substr(0, pos);
				buffer = buffer.substr(pos + 1);
				if (line[line.length() - 1] == '\r')
					line = line.substr(0, line.length() - 1);

				if (state == 0)
				{
					unsigned int i = 0;
					for (; i < line.length() && isalpha(line[i]); ++i)
						req.method += line[i];
					if (i >= line.length() || line[i] != ' ')
					{
						closesocket(_socket);
						return ;
					}
					while (line[i] == ' ')
						++i;
					for (; i < line.length() && line[i] != ' '; ++i)
						req.resource += line[i];
					if (i >= line.length() || line[i] != ' ')
					{
						closesocket(_socket);
						return ;
					}
					while (line[i] == ' ')
						++i;
					if (line.substr(i, 5) != "HTTP/")
					{
						closesocket(_socket);
						return ;
					}
					i += 5;
					for (; i < line.length() && line[i] != ' '; ++i)
						req.version += line[i];

					state = 1;
				}
				else if (state == 1)
				{
					if (line.empty())
						state = 2;
					else
					{
						unsigned int i = 0;
						std::string headerName;
						for (; i < line.length() && line[i] != ':'; ++i)
							headerName += tolower(line[i]);
						if (i >= line.length() || line[i] != ':')
						{
							closesocket(_socket);
							return ;
						}
						++i;
						while (line[i] == ' ')
							++i;
						req.headers[headerName] = line.substr(i);
					}
				}
			}
		}
		if (state >= 2)
		{
			if (contentLength == 0 && req.headers.find("content-length") != req.headers.end())
				contentLength = atoi(req.headers["content-length"].c_str());
			req.body += buffer;
			buffer = "";
			if (req.body.length() >= contentLength)
				break ;
		}
	}

	// PARSING OF THE GET STRING
	int pos;
	if ((pos = req.resource.find_first_of('?')) != std::string::npos)
	{
		req.getString = req.resource.substr(pos + 1);
		req.resource = req.resource.substr(0, pos);
		std::string getString = req.getString;
		do
		{
			std::string str;
			if ((pos = getString.find_first_of('&')) != std::string::npos)
			{
				str = getString.substr(0, pos);
				getString = getString.substr(pos + 1);
			}
			else
			{
				str = getString;
				getString = "";
			}
			if ((pos = str.find_first_of('=')) != std::string::npos)
				req.get[str.substr(0, pos)] = str.substr(pos + 1);
		}
		while (!getString.empty());
	}

	handleRequest(connection, req);

	WaitForSingleObject(_childrenThreadsMutex, INFINITE);
	closesocket(connection);
	_childrenThreads.remove(HSPair(GetCurrentThread(), connection));
	ReleaseMutex(_childrenThreadsMutex);

}
