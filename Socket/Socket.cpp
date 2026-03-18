#include "Socket.hpp"

int Socket::errorNumber = 0;

int Socket::inetConnect(const string &host, const string &service, int type)
{
	addrinfo hints, *result, *rp;
	char *h = NULL, *s = NULL;
	int sock = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = type;

	if (host != "")
		h = (char *)host.c_str();
	if (service != "")
		s = (char *)service.c_str();

	if (getaddrinfo(h, s, &hints, &result) != 0)
	{
		
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock == -1)
			continue;
		int res = connect(sock, rp->ai_addr, rp->ai_addrlen);
		if (res != -1)
			break;
		
		close(sock);
	}
	freeaddrinfo(result);
	
	return (rp != NULL) ? sock : -1;
}

int Socket::inetPassiveSocket(const char *host, const char *service, int type,
							bool doListen, int backlog)
{
	addrinfo hints, *result, *rp;
	int sock = 0, optVal = 1, res;
	(void)res;
	(void)optVal;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = type;
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(host, service, &hints, &result) != 0)
		return -1;

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sock = socket(rp->ai_family, rp->ai_socktype | O_NONBLOCK, rp->ai_protocol);
		if (sock == -1)
			continue;
		if (doListen)
		{
			if (rp->ai_family == AF_INET6)
				res = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &optVal, sizeof(optVal));
			res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));
			res += setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optVal, sizeof(optVal));
			if (res != 0)
			{
				close(sock);
				freeaddrinfo(result);
				return -1;
			}
		}
		if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		if (sock > 0)
			close(sock);
		sock = -1;
		break;
	}
	freeaddrinfo(result);
	if (doListen && rp != NULL && sock > 0)
	{
		if (listen(sock, backlog) == -1)
		{
			close(sock);
			return -1;
		}
	}
	return (rp != NULL) ? sock : -1;
}

int Socket::inetListen(const string &host, const string &service, int backlog)
{
	const char *h;
	if (host == "")
		h = NULL;
	else
		h = host.c_str();
	int result = inetPassiveSocket(h, service.c_str(), SOCK_STREAM, true, backlog);
	
	return result;
}

int Socket::inetBind(const string &host, const string &service, int type)
{
	return inetPassiveSocket(host.c_str(), service.c_str(), type, false, 0);
}

string Socket::inetAddressStr(sockaddr *addr, socklen_t addrlen)
{
	stringstream addrStr;
	char host[NI_MAXHOST], service[NI_MAXSERV];
	errorNumber = 0;

	if (getnameinfo(addr, addrlen, host, NI_MAXHOST, service,
					NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV) == 0)
		addrStr << "(" << host << ", " << service << ")";
	else
		addrStr << "UNKNOWN";

	errorNumber = 1;
	return addrStr.str();
}

void Socket::inetAddressStr(sockaddr *addr, socklen_t addrlen, string &host, string &port)
{
	stringstream addrStr;
	char _host[NI_MAXHOST], service[NI_MAXSERV];
	errorNumber = 0;

	if (getnameinfo(addr, addrlen, _host, NI_MAXHOST, service,
					NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV) == 0)
	{
		host = _host;
		port = service;
	}
	else
		errorNumber = 1;
}

sockaddr_storage Socket::getLocalAddress(int sock)
{
	sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	errorNumber = 0;

	if (getsockname(sock, (sockaddr *)&addr, &len) == -1)
		errorNumber = 1;
	return addr;
}

string Socket::getLocalName(int sock)
{
	sockaddr_storage addr = getLocalAddress(sock);

	return inetAddressStr((sockaddr *)&addr, sizeof(addr));
}

sockaddr_storage Socket::getRemoteAddress(int sock)
{
	sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	errorNumber = 0;

	if (getpeername(sock, (sockaddr *)&addr, &len) == -1)
		errorNumber = 1;
	return addr;
}

string Socket::getRemoteName(int sock)
{
	sockaddr_storage addr = getRemoteAddress(sock);

	return inetAddressStr((sockaddr *)&addr, sizeof(addr));
}

void Socket::getRemoteName(int sock, string &host, string &port)
{
	sockaddr_storage addr = getRemoteAddress(sock);

	inetAddressStr((sockaddr *)&addr, sizeof(addr), host, port);
}

void Socket::getLocalName(int sock, string &host, string &port)
{
	sockaddr_storage addr = getLocalAddress(sock);

	inetAddressStr((sockaddr *)&addr, sizeof(addr), host, port);
}

int Socket::getSocketType(int sock)
{
	int optval;
	socklen_t optlen;

	optlen = sizeof(optval);
	if (getsockopt(sock, SOL_SOCKET, SO_TYPE, &optval, &optlen) == -1)
		return -1;
	return optval;
}

Socket::Socket(int sock): fd(sock)
{
}

string Socket::getIpByHost(const string &host, const string &port, int type)
{
    addrinfo hints, *result;
    string ip = "";
    string Port = ""; 
	const char *h = NULL, *p = NULL;

	if (host != "")
		h = host.c_str();
	if (port != "")
		p = port.c_str();

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = type;     
    hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(h, p, &hints, &result) != 0)
        return "";

    if (result != NULL) {
        inetAddressStr(result->ai_addr, result->ai_addrlen, ip, Port);
    }
	else {
		return "";
	}

    freeaddrinfo(result);
    return ip + ":" + Port;
}

Socket::~Socket()
{
	close(fd);
}

