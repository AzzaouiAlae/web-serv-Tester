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
	bool passed;
	TestCase (): sendedBytes(0), passed(false) {
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
	static vector<ATestList *> _testLists;

protected:
	int _failedTests;
	int _passedTests;
	vector<pair<string, void (ATestList::*)()> > _testFunctions;
	Multiplexer multiplexer;
	static bool GetContentLength(string &response, size_t &contentLength);
	static bool GetResponseHeaderLength(string &response, size_t &headerLength);
	bool SendRequestToServer(TestCase &config);
	bool ReadResponseFromServer(TestCase &config);
	bool connectToServer(TestCase &config);
	void printTestCard(TestCase &config);
	void actServerResponse(TestCase &config);
	void printServerResponseHeader(TestCase &config);
	void RunTestCase(TestCase &config);
	void PrintTestResult();
	void ResetTestResults();
public:
	int getFailedTests() const;
	int getPassedTests() const;
	static void preperForNextTest();
	static int readIntegerInput();
	static string readInput();
	virtual void AddAllTests() = 0;
	virtual void performTestCase(int choice);
	virtual void RunAllTests();
	virtual void ShowTestsList();
	ATestList(const string &name);
	string getName() const;
	virtual ~ATestList();
	static vector<ATestList *> getTestLists();
};
