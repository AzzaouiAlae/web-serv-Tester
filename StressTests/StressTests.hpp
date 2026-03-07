#pragma once
#include "../ATestList/ATestList.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

class StressTests : public ATestList
{
	void RapidSequentialRequestsTest();
	void ConcurrentConnectionsTest();
	void SlowClientTest();
	void LargeRequestHeadersTest();
	void ConnectionAfterErrorTest();
	void AddAllTests();

	// Opens a raw POSIX socket, sends request, reads until Content-Length+headers
	// are fully received.  Uses SO_RCVTIMEO (timeoutMs) instead of epoll —
	// safe to call from forked children and from any thread because it shares
	// no state with the ATestList multiplexer instance.
	// Returns true if a complete response was received; false on any error.
	static bool rawRequest(const string &host, const string &port,
	                       const string &request, string &responseOut,
	                       int timeoutMs);

	// Synthesises pass/fail accounting for tests that bypass RunTestCase.
	// Builds a fake TestCase whose response field is set to syntheticResponse,
	// then delegates to actServerResponse() for consistent pass/fail tracking
	// and header printing.
	void evaluateStressResult(const string &testName,
	                          const string &testDescription,
	                          const string &expectedResponse,
	                          const string &syntheticResponse);
public:
	StressTests();
	~StressTests();
};
