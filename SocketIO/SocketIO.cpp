#include "SocketIO.hpp"

int SocketIO::errorNumber = 0;

int SocketIO::Send(void *buff, int size)
{




	return write(fd, buff, size);
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
	return n;
}

ssize_t SocketIO::FileToSocket(int fileFd, int size)
{
	return sendfile(this->fd, fileFd, NULL, size);
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
}

SocketIO::~SocketIO()
{
	close(fd);
}

int SocketIO::GetFd()
{
	return fd;
}