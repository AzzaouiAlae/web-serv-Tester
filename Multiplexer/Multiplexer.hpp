#pragma once
#include "../SocketIO/SocketIO.hpp"
#include "../Headers.hpp"
#include <sys/epoll.h>

class Multiplexer
{
	int count;
	int epollFd;
	void epoolInit();
	static Multiplexer *currentMultiplexer;
	int timeout;
	bool stopEvntLoop;
public:
	epoll_event *eventList;
	Multiplexer();
	~Multiplexer();
	int epollWait(int timeout);
	bool AddAsEpoll(SocketIO *fd, int type);
	bool AddAsEpollIn(SocketIO *fd);
	bool AddAsEpollOut(SocketIO *fd);
	bool ChangeToEpollIn(SocketIO *fd);
	bool ChangeToEpollOut(SocketIO *fd);
	void ChangeToEpollInOut(SocketIO *fd);
	bool ChangeToEpollOneShot(SocketIO *fd);
	bool DeleteFromEpoll(SocketIO *fd);
	static Multiplexer *GetCurrentMultiplexer();
};
