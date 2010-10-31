#pragma once

#include <Winsock2.h>
#include <Windows.h>
#include <list>
#include <map>

typedef std::pair<HANDLE, SOCKET> HSPair;
typedef std::list<HSPair> HANDLEList;

class SoHTTP
{
public:
	SoHTTP();
	virtual ~SoHTTP(void);

	bool	start(int port, int backlog);
	void	stop();

	void	sendFromDir(SOCKET connection, const char * dir, const char * resource);

	struct Request
	{
		typedef std::map<std::string, std::string> headerMap, getMap;

		std::string method;
		std::string resource;
		std::string getString;
		getMap get;
		std::string version;

		headerMap headers;

		std::string body;
	};


protected:
	virtual void handleRequest(SOCKET connection, Request request) = 0;

private:
	void	_loop();
	void	_handleConnection(SOCKET connection);

	HANDLE	_thread;
	SOCKET	_socket;
	HANDLEList	_childrenThreads;
	HANDLE		_childrenThreadsMutex;

	friend DWORD WINAPI startLoop(__in LPVOID lpParameter);
	friend DWORD WINAPI handleConnection(__in LPVOID lpParameter);
};
