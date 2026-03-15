#pragma once 
#include "../Headers.hpp"

#define CLOSE_TIME 10
#define TIMEOUT 20

using namespace std;

enum IOState {
	ePipe1 = 1,
	ePipe0 = 2,
	eSocket = 4,
};

#define KBYTE 1024 * 1024
#define MBYTE 50

class SocketIO {
	static long CurrentTime();
	bool pipeInitialized;
	int pendingInPipe;
	int status;
	int SendedBuffToPipe;
	char *buff;
	int fd;
public:
	int GetFd();
	int pipefd[2];
	SocketIO(int fd);
    ~SocketIO();
	int SendBuffToPipe(void *buff, int size);
	static int errorNumber;
	int Send(void *buff, int size);
	ssize_t FileToSocket(int fileFd, int size);
	int SocketToFile(int fileFD, int size);
	int SocketToSocketRead(int socket, int size);
	int SocketToSocketWrite(int socket, int size);
	static void ClearPipePool();
	int SendSocketToPipe(int size = KBYTE * MBYTE);
};