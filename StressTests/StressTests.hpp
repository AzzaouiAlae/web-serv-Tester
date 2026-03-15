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
	void KeepAliveConnectionEvictionTest();
	void AddAllTests();

	// Uses ATestList connection/send/read flow for a complete HTTP exchange.
	// This guarantees response reading follows ReadResponseFromServer logic.
	bool requestWithATestList(const string &host, const string &port,
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
