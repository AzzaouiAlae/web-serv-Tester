#pragma once
#include "../Headers.hpp"
#include "../Multiplexer/Multiplexer.hpp"
#include "../Socket/Socket.hpp"

struct TestCase
{
	string name;
	string description;
	string port;
	string host;
	string request;
	vector<string> expectedResponse;
	string configurationsForTestCase;
	int timeout;
	int socket;
	SocketIO *socketIO;
	size_t sendedBytes;
	char *responseBuffer;
	string response;
	string streamedResponseBodySummary;
	size_t streamedResponseBodyBytes;
	size_t streamedRequestPayloadSize;
	size_t streamedRequestHeaderSize;
	size_t streamedResponseHeaderSize;
	size_t contentLength;
	size_t headerLength;
	bool passed;
	bool printTest;
	int sleepTime;
	size_t maxSend;
	int pipeFd[2];
	vector<pid_t> childPids;
	vector<int> childResults;
	bool parentProcess;
	int forkCount;
	int totalRequests;
	string body;
	// Progressive body generation tracking
	size_t bodyGeneratedBytes;
	size_t bodyTotalSize;
	string bodyDescription;
	bool isBodyGenerationComplete;
	// Chunked encoding state
	bool chunkGenerated;
	size_t chunkSize;
	string chunkedBodyStart;
	string chunkedBodyEnd;
	size_t chunkedBodyStartSentBytes;
	size_t chunkedBodyEndSentBytes;
	size_t chunksRemaining;
	bool sendingEndChunk;
	bool isSubTest;
	int childIndex; // For tracking which child process is sending in forked tests
	TestCase() : sendedBytes(0), streamedResponseBodyBytes(0), streamedRequestPayloadSize(0), streamedRequestHeaderSize(0), streamedResponseHeaderSize(0), passed(false), printTest(true), bodyGeneratedBytes(0), bodyTotalSize(0),
				 isBodyGenerationComplete(false), chunkGenerated(false), chunkSize(32768),
				 chunkedBodyStartSentBytes(0), chunkedBodyEndSentBytes(0), chunksRemaining(0), sendingEndChunk(false)
	{
		responseBuffer = new char[KBYTE];
		contentLength = -1;
		headerLength = -1;
		socketIO = NULL;
		parentProcess = true;
		childIndex = -1;
		isSubTest = false;
		sleepTime = 100;
		maxSend = 50;
	}
	~TestCase()
	{
		delete[] responseBuffer;
		if (socketIO)
		{
			delete socketIO;
		}
	}
};

class ATestList
{
	string _name;
	static vector<ATestList *> _testLists;
	bool isListOfTests;
	int _failedTests;
	int _passedTests;
	bool _showSingleTestDetails;
	bool _reRunTest;
	static bool GetContentLength(string &response, size_t &contentLength);
	static bool GetResponseHeaderLength(string &response, size_t &headerLength);
	static bool IsChunkedTransferEncoding(const string &response, size_t headerLength);
	static bool HasChunkedTerminator(const string &response, size_t headerLength);
	static bool DecodeChunkedResponseBody(string &response, size_t headerLength);

	void printServerResponseHeader(TestCase &config);
	void ResetTestResults();
	void rePrintTest(TestCase &config);
	
	void printForkChildTestResult(TestCase &config, int totalSuccessCount, bool anyChildFailed, int failedChildIndex);
	void getChildResults(TestCase &config);
	bool runChildTestCase(TestCase &childConfig);
	bool forkChildProcess(TestCase &config);
	bool createPipe(TestCase &config);
	bool matchesBodyPattern(const string &responseBody, const string &pattern);
	size_t getBodyTotalSize(const string &description);
	string generateChunkStartHeader(const string &description, size_t &outChunkSize);
	string generateBodySegment(char bodyChar, size_t segmentSize);
	void CreateChunkedBody(TestCase &config);
	int SendChunkedBody(TestCase &config);



protected:
	vector<pair<string, void (ATestList::*)()>> _testFunctions;
	Multiplexer multiplexer;
	bool connectToServer(TestCase &config);
	void printTestCard(TestCase &config);
	void actServerResponse(TestCase &config);
	void RunTestCase(TestCase &config);
	void RunStreamingTestCase(TestCase &config);
	bool SendRequestToServer(TestCase &config);
	bool ReadResponseFromServer(TestCase &config);
	void RunForkChildTestCase(TestCase &config);
public:
	void PrintTestResult();
	static long CurrentTime();
	static string GetRandem();
	int getFailedTests() const;
	int getPassedTests() const;
	static void preperForNextTest();
	static int readIntegerInput();
	static string readInput();
	static vector<int> parseChoices(const string &input);
	virtual void AddAllTests() = 0;
	virtual void performTestCase(int choice);
	virtual void RunAllTests();
	virtual void ShowTestsList();
	ATestList(const string &name);
	string getName() const;
	virtual ~ATestList();
	static vector<ATestList *> getTestLists();
};
