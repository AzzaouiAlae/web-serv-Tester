#include "SocketIO.hpp"


int SocketIO::errorNumber = 0;

vector<pair<int, int> > SocketIO::pipePool;

priority_queue<SocketIO*, vector<SocketIO*>, SocketIO::CompareTimeout> SocketIO::timeoutList;


int SocketIO::Send(void *buff, int size)
{
	return write(fd, buff, size);
	// char *b = (char *)buff;
	// ssize_t sent = 0;
	// int flag = (ePipe0 | eSocket);
	// if (&(b[0]) != &((this->buff)[0]))
	// {
	// 	SendedBuffToPipe = 0;
	// 	this->buff = (char *)buff;
	// }

	// if (status & ePipe1 && SendedBuffToPipe < size)
	// {
	// 	sent = SendBuffToPipe(&((this->buff)[SendedBuffToPipe]), size - SendedBuffToPipe);
	// 	if (sent > 0)
	// 	{
	// 		SendedBuffToPipe += sent;
	// 		sent = 0;
	// 	}
	// }
	// if ((status & flag) == flag)
	// {
	// 	sent = SendPipeToSock();
	// }
	// return sent;
}

int SocketIO::SendBuffToPipe(void *buff, int size)
{
	struct iovec iov;
	iov.iov_base = buff;
	iov.iov_len  = size;

	ssize_t n = vmsplice(pipefd[1], &iov, 1, SPLICE_F_NONBLOCK);
	status &= ~ePipe1;
	if (n <= 0)
		return n;
	pendingInPipe += n;
	return n;
}

int SocketIO::SendPipeToSock()
{
	if (pendingInPipe <= 0)
		return 0;
	ssize_t sent = splice(pipefd[0], NULL, this->fd, NULL, pendingInPipe, SPLICE_F_NONBLOCK);
	status &= ~(ePipe0 | eSocket);
	if (sent < 0)
		return -1;
	pendingInPipe -= sent;
	return sent;
}



ssize_t SocketIO::FileToSocket(int fileFd, int size)
{
	return sendfile(this->fd, fileFd, NULL, size);
}

int SocketIO::GetPipePoolSize()
{
	return pipePool.size();
}

int SocketIO::CloseSockFD(int fd)
{
	static vector<pair<int, long> > fds;
	long now = CurrentTime();
	int optval = 0;
	tcp_info info;
    socklen_t len;

	if (fd != -1)
	{
		setsockopt(fd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));
		shutdown(fd, SHUT_WR);
		fds.push_back(pair<int, long>(fd, now));
	}
	
	for(int i = 0; i < (int)fds.size(); i++)
	{
		len = sizeof(info);
		if (getsockopt(fds[i].first, IPPROTO_TCP, TCP_INFO, &info, &len) == 0)
		{
			if (info.tcpi_unacked == 0)
			{
				fds[i].second -= USEC;
			}
		}
		if (fds[i].second + USEC * CLOSE_TIME < now)
		{
			close(fds[i].first);
			fds[i] = fds.back();
            fds.pop_back();
			i--;
		}
	}
	return fds.size();
}

long SocketIO::CurrentTime()
{
	timeval time;
	gettimeofday(&time, NULL);
	return time.tv_sec * USEC + time.tv_usec;
}

int SocketIO::SocketToFile(int fileFD, int size)
{
	int len = 0, flag = (eSocket | ePipe1);

	if ((status & flag) == flag)
	{
		len = splice(this->fd, NULL, pipefd[1], NULL, size, 0);
		status &= ~flag;
		if (len > 0)
			pendingInPipe += len;
		len = 0;
	}
	if (status & ePipe0 && pendingInPipe > 0)
	{
		len = splice(pipefd[0], NULL, fileFD, NULL, pendingInPipe, 0);
		status &= ~ePipe0;
		if (len == -1)
			return -1;
		pendingInPipe -= len;
	}
	
	return len;
}

int SocketIO::SendSocketToPipe(int size)
{
	int len = 0;
	if (status & ePipe1) {
		len = splice(fd, NULL, pipefd[1], NULL, size, 0);
		status &= ~ePipe1;
		if (len == -1)
			return -1;
		pendingInPipe += len;
		
	}
	return len;
}

int SocketIO::SocketToSocketRead(int socket, int size)
{
	int len = 0, flag = (eSocket | ePipe0);

	if (status & ePipe1)
	{
		len = splice(socket, NULL, pipefd[1], NULL, size, 0);
		status &= ~ePipe1;
		if (len == -1)
			return -1;
		pendingInPipe += len;
	}
	if ((status & flag) == flag && pendingInPipe > 0)
	{
		len = splice(pipefd[0], NULL, this->fd, NULL, pendingInPipe, 0);
		status &= ~flag;
		if (len == -1)
			return -1;
		pendingInPipe -= len;
	}
	return len;
}

int SocketIO::SocketToSocketWrite(int socket, int size)
{
	int len = 0, flag = (eSocket | ePipe1);

	if ((status & flag) == flag)
	{
		len = splice(this->fd, NULL, pipefd[1], NULL, size, 0);
		status &= ~flag;
		if (len == -1)
			return -1;
		pendingInPipe += len;
	}
	if (status & ePipe0 && pendingInPipe > 0)
	{
		len = splice(pipefd[0], NULL, socket, NULL, pendingInPipe, 0);
		status &= ~ePipe0;
		if (len == -1)
			return -1;
		pendingInPipe -= len;
	}
	return len;
}

SocketIO::SocketIO(int fd): pipeInitialized(false), pendingInPipe(0), status(0), fd(fd)
{
	buff = NULL;
	if (pipePool.size() > 0)
	{
		pair<int, int> p = pipePool[pipePool.size() - 1];
		pipefd[0] = p.first;
		pipefd[1] = p.second;
		pipePool.pop_back();
		pipeInitialized = true;
		
	}
	else if (pipe2(pipefd, O_NONBLOCK) == -1)
	{
		SocketIO::errorNumber = 1;
		
	}
	else
	{
		pipeInitialized = true;
		
	}
	lastTime = time(NULL);
	timeout = TIMEOUT;
	timeoutList.push(this);
}

SocketIO::~SocketIO()
{
	
	CloseSockFD(this->fd);
	if (pendingInPipe == 0 && pipeInitialized && pipePool.size() < 100)
	{
		pipePool.push_back(pair<int, int>(pipefd[0], pipefd[1]));
		
	}
	else if (pipeInitialized)
	{
		close(pipefd[0]);
		close(pipefd[1]);
		
	}
}

void SocketIO::ClearPipePool()
{
	for(int i = 0; i < (int)pipePool.size(); i++)
	{
		close(pipePool[i].first);
		close(pipePool[i].second);
	}
}

bool SocketIO::CompareTimeout::operator()(const SocketIO *a, const SocketIO *b) const 
{
    return (a->GetEndTime()) > (b->GetEndTime());
}

time_t SocketIO::GetEndTime() const
{
	return lastTime + timeout;
}

void SocketIO::UpdateTime()
{
	lastTime = time(NULL);
}

int SocketIO::GetFd()
{
	return fd;
}