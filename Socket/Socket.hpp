#pragma once 

#include "../Headers.hpp"

using namespace std;

class Socket  {
	static int inetPassiveSocket(const char *host, const char *service, int type, 
		bool doListen, int backlog);
	int fd;
public:
	int acceptedSocket;
	static int errorNumber;
	static int inetConnect(const string& host, const string& service, int type);
	static int inetListen(const string& host, const string& service, int backlog);
	static int inetBind(const string& host, const string& service, int type);
	static string inetAddressStr(sockaddr *addr, socklen_t addrlen);
	static void inetAddressStr(sockaddr *addr, socklen_t addrlen, string &host, string &port);
	static sockaddr_storage getLocalAddress(int sock);
	static sockaddr_storage getRemoteAddress(int sock);
	static string getLocalName(int sock);
	static string getRemoteName(int sock);
	static void getLocalName(int sock, string &host, string &port);
	static void getRemoteName(int sock, string &host, string &port);
	static int getSocketType(int sock);
	static string getIpByHost(const string& host, const string &port, int type = AF_INET);

	Socket(int sock);
	~Socket();
};
