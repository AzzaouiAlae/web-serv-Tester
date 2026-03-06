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
	static vector<pair<int, int> > pipePool;
	bool pipeInitialized;
	int pendingInPipe;
	int status;
	int SendedBuffToPipe;
	char *buff;
	time_t timeout;
	time_t lastTime;
	int fd;
public:
	int GetFd();
	void UpdateTime();
	time_t GetEndTime() const;
	static void clearTimeout();
	static int GetPipePoolSize();
	int pipefd[2];
	SocketIO(int fd);
    ~SocketIO();
	int SendBuffToPipe(void *buff, int size);
	int SendPipeToSock();
	static int errorNumber;
	void SetStateByFd(int fd);
	int Send(void *buff, int size);
	ssize_t FileToSocket(int fileFd, int size);
	int SocketToFile(int fileFD, int size);
	int SocketToSocketRead(int socket, int size);
	int SocketToSocketWrite(int socket, int size);
	static int CloseSockFD(int fd);
	static void ClearPipePool();
	int SendSocketToPipe(int size = KBYTE * MBYTE);
	struct CompareTimeout {
		bool operator()(const SocketIO *a, const SocketIO *b) const ;
	};
private:
	static priority_queue<SocketIO*, vector<SocketIO*>, SocketIO::CompareTimeout> timeoutList;
};