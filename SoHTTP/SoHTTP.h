/// \file
/// Salomon Brys HHTP Library Header
/// License LGPL
/// \author Salomon BRYS <salomon.brys@gmail.com>

/// \addtogroup SoHHTP
/// SoHTTP is a simple library to handle HTTP requests on Windows
/// \{

#pragma once

#include <Winsock2.h>
#include <Windows.h>
#include <list>
#include <map>

typedef std::pair<HANDLE, SOCKET> HSPair;
typedef std::list<HSPair> HANDLEList;

/// \author Salomon BRYS <salomon.brys@gmail.com>
/// Salomon Brys HHTP Library Class.
/// Any web server has to subclass this. It then has to implements the handleRequest method.
/// SoHTTP only parses requests, it does not handle the response.
/// Once a request is parsed, the request structure and the socket are passed to handleRequest.
/// It is the subclass job, via handleRequest, to write any response, HTTP or not, into the socket.
class SoHTTP
{
public:
	/// Constructor
	SoHTTP();
	
	/// Virtual destructor
	virtual ~SoHTTP(void);

	/// Starts a server in its own thread.
	/// \param[in]	port	The port to listen connections in.
	/// \param[out]	backlog	The maximum length of the queue of pending connections.
	/// \return Wether the starting of the server has succeded or not
	bool	start(int port, int backlog);
	
	/// Stops the running server.
	/// Terminates all the threads of connected clients
	/// and terminates the server listening thread.
	void	stop();

	/// Utility function to send files in a socket.
	/// This utility function is meant to be called from a handleRequest implementation if the implemented server needs to display file contents.
	/// It handles a HTTP response with only 404 and 200 error codes.
	///  - If the given resource is a file, then its content is directly sent to the socket (after the HTTP 200 header).
	///  - Else, if the given resource is a directory :
	///     - if a index.html file exists, it is sent.
	///     - if not, error 404.
	///  - Else, error 404.
	/// The true real path of the file to display is dir\\resource
	/// \param[in]	connection	The socket to write to.
	/// \param[in]	dir			The path of the directory of the resource.
	/// \param[in]	resource	The resource.
	void	sendFromDir(SOCKET connection, const char * dir, const char * resource);

	/// The Request structure with every HTTP header and request information.
	struct Request
	{
		typedef std::map<std::string, std::string> headerMap, getMap;

		/// The method name. Should usualy be GET or POST but could be any given method by the client
		std::string method;
		
		/// The resource that is requested, without the get variable string.
		std::string resource;
		
		/// If the resource has a get variable string (e.g. "?a=b&c=d"), contains the raw get variable string.
		std::string getString;
		
		/// If the resource has a get variable string (e.g. "?a=b&c=d"), contains all parsed get variables.
		getMap get;
		
		/// The HTTP version given. Should only be 0.9, 1.0 or 1.1 but really depends on the client.
		std::string version;

		/// The name and values of each given HTTP header.
		headerMap headers;

		/// The given body of the request, if any.
		std::string body;
	};


protected:
	/// Method to be implemented by any server subclass that uses SoHTTP.
	/// The socket will be closed when the function returns.
	/// This function can block without impacting the server as it is run in a thread.
	/// \param[in]	connection	The socket connected to the clients.
	/// \param[in]	request		The request informations passed by reference for optimization. Is not used after the handleRequest call so it can be modified without side effects.
	virtual void handleRequest(SOCKET connection, Request & request) = 0;

private:
	/// The listening loop of the main server thread.
	/// Handles the new connections, creating their own thread and calls _handleConnection.
	void	_loop();
	
	/// Handles a connection life cycle.
	///  - Parses a request, creating a Request structure.
	///  - Calls handleRequest.
	///  - Terminates the connection.
	void	_handleConnection(SOCKET connection);

	/// The handle of the main thread (listening loop).
	HANDLE		_thread;
	
	/// The socket of the main thread (listening loop).
	SOCKET		_socket;
	
	/// The list of all current connections threads.
	/// Used by each connection thread to register and unregister themselves.
	HANDLEList	_childrenThreads;
	
	/// The mutex to access _childrenThreads.
	HANDLE		_childrenThreadsMutex;

	friend DWORD WINAPI startLoop(__in LPVOID lpParameter);
	friend DWORD WINAPI handleConnection(__in LPVOID lpParameter);
};

/// \}
