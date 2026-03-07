#pragma once
#include "../Headers.hpp"
#include "../Multiplexer/Multiplexer.hpp"
#include "../Socket/Socket.hpp"

struct TestCase {
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
	TestCase (): sendedBytes(0) {
		responseBuffer = new char[KBYTE];
		contentLength = -1;
		headerLength = -1;
	}
	~TestCase() {
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
	bool SendRequestToServer(TestCase &config);
	bool ReadResponseFromServer(TestCase &config);
	bool connectToServer(TestCase &config);
	void printTestCard(TestCase &config);
	void actServerResponse(TestCase &config);
	void printServerResponseHeader(const string &response);
	void preperForNextTest();
	void RunTestCase(TestCase &config);
public:
	virtual void ShowTestsList() = 0;
	ATestList(const string &name);
	string getName() const;
	virtual ~ATestList();
	static vector<ATestList *> getTestLists();
};