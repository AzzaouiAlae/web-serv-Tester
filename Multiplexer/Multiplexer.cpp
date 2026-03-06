#include "Multiplexer.hpp"

Multiplexer* Multiplexer::currentMultiplexer;

Multiplexer* Multiplexer::GetCurrentMultiplexer()
{
	return currentMultiplexer;
}

Multiplexer::Multiplexer()
{
	currentMultiplexer = this;
	count = 0;

	epoolInit();
}

void Multiplexer::epoolInit()
{
	epollFd = epoll_create1(EPOLL_CLOEXEC);
	eventList = new epoll_event[1024];
}

bool Multiplexer::AddAsEpollIn(SocketIO *fd)
{
	return AddAsEpoll(fd, EPOLLIN);
}

bool Multiplexer::AddAsEpollOut(SocketIO *fd)
{
	return AddAsEpoll(fd, EPOLLOUT);
}

bool Multiplexer::AddAsEpoll(SocketIO *fd, int type)
{
	epoll_event ev;

	ev.events = type;
	ev.data.ptr = (void *)fd;

	if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd->GetFd(), &ev) == -1)
	{
		return false;
	}
	count++;

	return true;
}

bool Multiplexer::ChangeToEpollOut(SocketIO *fd)
{
	epoll_event ev;

	ev.events = EPOLLOUT;
	ev.data.ptr = (void *)fd;

	if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd->GetFd(), &ev) == -1)
	{
		return false;
	}

	return true;
}

bool Multiplexer::ChangeToEpollIn(SocketIO *fd)
{
	epoll_event ev;

	ev.events = EPOLLIN;
	ev.data.ptr = (void *)fd;

	if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd->GetFd(), &ev) == -1)
	{
		return false;
	}

	return true;
}

bool Multiplexer::ChangeToEpollOneShot(SocketIO *fd)
{
	epoll_event ev;

	ev.events = EPOLLONESHOT;
	ev.data.ptr = (void *)fd;

	if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd->GetFd(), &ev) == -1)
	{
		return false;
	}

	return true;
}

bool Multiplexer::DeleteFromEpoll(SocketIO *fd)
{
	count--;
	return epoll_ctl(epollFd, EPOLL_CTL_DEL, fd->GetFd(), NULL);
}

void Multiplexer::ChangeToEpollInOut(SocketIO *fd)
{
    struct epoll_event ev;
	
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = fd->GetFd();
    epoll_ctl(epollFd, EPOLL_CTL_MOD, fd->GetFd(), &ev);
}

int Multiplexer::epollWait(int timeout)
{
	return epoll_wait(epollFd, eventList, 1024, timeout);
}

Multiplexer::~Multiplexer()
{
	close(epollFd);
	if (currentMultiplexer == this)
		currentMultiplexer = NULL;
	delete[] eventList;
}
