#pragma once
#include "../Headers.hpp"
#include "../Multiplexer/Multiplexer.hpp"
#include "../Socket/Socket.hpp"

struct TestConfig {
	string name;
	string description;
	string port;
	string host;
	string request;
	string expectedResponse;
	string configFileData;
	int timeout;
	int socket;
	SocketIO *socketIO;
	size_t sendedBytes;
	char *responseBuffer;
	string response;
	size_t contentLength;
	size_t headerLength;
	TestConfig (): sendedBytes(0) {
		responseBuffer = new char[KBYTE];
		contentLength = -1;
		headerLength = -1;
	}
	~TestConfig() {
		delete[] responseBuffer;
	}
};

class ATestList {
	
	string _name;
	int nbTests;
	static vector<ATestList *> _testLists;
protected:
	Multiplexer multiplexer;
	static bool GetContentLength(string &response, size_t &contentLength);
	static bool GetResponseHeaderLength(string &response, size_t &headerLength);
	bool SendRequestToServer(TestConfig &config);
	bool ReadResponseFromServer(TestConfig &config);
	bool connectToServer(TestConfig &config);
	void printTestCard(TestConfig &config);
	void actServerResponse(TestConfig &config);
	void printServerResponseHeader(const string &response);
public:
	virtual void ShowTestsList() = 0;
	ATestList(const string &name);
	string getName() const;
	virtual ~ATestList();
	static vector<ATestList *> getTestLists();
};