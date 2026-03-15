#include "StressTests.hpp"

namespace {

static void cleanupStressSocket(Multiplexer &multiplexer, TestCase &cfg)
{
	if (cfg.socketIO)
	{
		multiplexer.DeleteFromEpoll(cfg.socketIO);
		delete cfg.socketIO;
		cfg.socketIO = NULL;
	}
	cfg.socket = -1;
}

static bool childConcurrentRequest(const string &host, const string &port,
	const string &request, string &responseOut, int timeoutMs)
{
	responseOut.clear();
	int fd = Socket::inetConnect(host, port, SOCK_STREAM);
	if (fd == -1)
		return false;

	struct timeval tv;
	tv.tv_sec = timeoutMs / 1000;
	tv.tv_usec = (timeoutMs % 1000) * 1000;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	size_t sent = 0;
	while (sent < request.size())
	{
		ssize_t n = send(fd, request.c_str() + sent, request.size() - sent, 0);
		if (n <= 0)
		{
			close(fd);
			return false;
		}
		sent += (size_t)n;
	}

	char buf[4096];
	while (true)
	{
		ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n <= 0)
			break;
		responseOut.append(buf, (size_t)n);

		size_t headerPos = responseOut.find("\r\n\r\n");
		if (headerPos == string::npos)
			continue;
		size_t headerLength = headerPos + 4;

		string loweredHeaders = responseOut.substr(0, headerLength);
		for (size_t i = 0; i < loweredHeaders.size(); i++)
			loweredHeaders[i] = (char)tolower((unsigned char)loweredHeaders[i]);

		if (loweredHeaders.find("transfer-encoding:") != string::npos
			&& loweredHeaders.find("chunked") != string::npos)
		{
			if (responseOut.find("0\r\n\r\n", headerLength) != string::npos)
				break;
			continue;
		}

		size_t clPos = loweredHeaders.find("content-length:");
		if (clPos == string::npos)
			continue;
		clPos += strlen("content-length:");
		while (clPos < loweredHeaders.size() && isspace(loweredHeaders[clPos]))
			clPos++;
		size_t clEnd = loweredHeaders.find("\r\n", clPos);
		if (clEnd == string::npos)
			continue;
		size_t contentLength = (size_t)atoi(loweredHeaders.substr(clPos, clEnd - clPos).c_str());
		if (responseOut.size() >= headerLength + contentLength)
			break;
	}

	close(fd);
	return !responseOut.empty();
}

}
// ─────────────────────────────────────────────────────────────────────────────
// WHY STRESS TESTS NOW USE THE ATESTLIST FLOW
//
// Per request, tests use connectToServer + SendRequestToServer +
// ReadResponseFromServer so all reads follow one completion policy.
//
// Concurrent tests still use fork(): each child has its own process state,
// including its own copy of the multiplexer instance.
//
// Pass/fail accounting is kept consistent through evaluateStressResult(),
// which builds a synthetic TestCase and calls actServerResponse() exactly
// once per logical test — the same codepath used by RunTestCase().
//
// ── SUBJECT QUOTE ────────────────────────────────────────────────────────────
// "Resilience is key. Your server must remain operational at all times."
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// Tuning constants
// ─────────────────────────────────────────────────────────────────────────────
static const int    SEQUENTIAL_COUNT    = 1000;    // requests in Test 1
static const int    CONCURRENT_CLIENTS  = 10;    // forked children in Test 2
static const int    SLOW_BYTE_DELAY_US  = 5000;  // 5 ms between bytes in Test 3
static const size_t LARGE_HEADER_SIZE   = 8192;  // bytes in custom header Test 4
static const int    RAW_TIMEOUT_MS      = 3000;  // timeout used by ATestList flow
static const int    KEEPALIVE_SWARM     = 10000; // sockets held in Test 6

// ─────────────────────────────────────────────────────────────────────────────
// requestWithATestList — helper
//
// Runs one request through ATestList connect/send/read helpers and returns
// the full response. Cleans up epoll registration and SocketIO on all paths.
// ─────────────────────────────────────────────────────────────────────────────
bool StressTests::requestWithATestList(const string &host, const string &port,
                                       const string &request, string &responseOut,
                                       int timeoutMs)
{
	TestCase cfg;
	cfg.host = host;
	cfg.port = port;
	cfg.request = request;
	cfg.timeout = timeoutMs;
	cfg.socket = -1;
	cfg.socketIO = NULL;

	if (!connectToServer(cfg))
		return false;

	if (!SendRequestToServer(cfg))
	{
		cleanupStressSocket(multiplexer, cfg);
		return false;
	}

	if (!ReadResponseFromServer(cfg))
	{
		cleanupStressSocket(multiplexer, cfg);
		return false;
	}

	responseOut = cfg.response;
	cleanupStressSocket(multiplexer, cfg);
	return !responseOut.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// evaluateStressResult — helper for tests that bypass RunTestCase
//
// Builds a TestCase whose response field is set to syntheticResponse, then
// calls actServerResponse() to perform the substring match and update
// _passedTests / _failedTests exactly as RunTestCase() does.
// ─────────────────────────────────────────────────────────────────────────────
void StressTests::evaluateStressResult(const string &testName,
                                       const string &testDescription,
                                       const string &expectedResponse,
                                       const string &syntheticResponse)
{
	TestCase result;
	result.name             = testName;
	result.description      = testDescription;
	result.expectedResponse.push_back(expectedResponse);
	result.response         = syntheticResponse;
	// Set headerLength so actServerResponse can call substr(0, headerLength)
	// without reading past the synthetic string.
	result.headerLength     = 
		min(syntheticResponse.size(),
		syntheticResponse.find("\r\n\r\n") != string::npos
		? syntheticResponse.find("\r\n\r\n") + 4
		: syntheticResponse.size());
	printTestCard(result);
	actServerResponse(result);
}

// ─────────────────────────────────────────────────────────────────────────────

StressTests::StressTests() : ATestList("Stress Tests")
{
	AddAllTests();
}

StressTests::~StressTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — Rapid sequential requests
// Targets: ServerManager (fd recycling), Multiplexer (epoll correctness after
//          repeated close/open cycles), Socket (no fd leak)
//
// Sends SEQUENTIAL_COUNT GET / requests back-to-back, each on its own
// new TCP connection, using the same ATestList send/read flow used elsewhere.
// A fresh connection is opened and closed for every iteration.
//
// Why this matters: a common fd leak occurs when a request fails mid-flight
// and the close() path is skipped.  After enough connections the server runs
// out of file descriptors and starts refusing new ones.  50 iterations is
// enough to surface a leak without making the test unbearably slow.
//
// Pass criterion: every single response contains "HTTP/1.1 200 OK".
// Even one failure means a connection was refused or produced garbage — the
// server lost state between requests.
//
// If iteration N succeeds but N+1 fails, the failure count pinpoints
// approximately when the server ran out of fds.
// ─────────────────────────────────────────────────────────────────────────────
void StressTests::RapidSequentialRequestsTest()
{
	// arrange
	const string host    = "localhost";
	const string port    = "1025";
	const string request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	const string expected = "HTTP/1.1 200 OK";

	cout << endl << "  " << CLI::runBadge() << "  " << CLR_STEP << "Rapid Sequential Requests" << RESET
	     << CLR_DIM << " (" << SEQUENTIAL_COUNT << " iterations)" << RESET << endl;

	int  successCount = 0;
	int  firstFailAt  = -1;
	string lastFailResponse;

	for (int i = 0; i < SEQUENTIAL_COUNT; i++)
	{
		string response;
		bool ok = requestWithATestList(host, port, request, response, RAW_TIMEOUT_MS);

		if (ok && response.find(expected) != string::npos)
		{
			successCount++;
		}
		else
		{
			if (firstFailAt == -1)
				firstFailAt = i + 1;
			lastFailResponse = response;
		}
	}

	// Build a diagnostic synthetic response for evaluateStressResult
	string synthetic;
	if (successCount == SEQUENTIAL_COUNT)
	{
		// All passed — produce a string that contains the expected substring
		synthetic = expected;
	}
	else
	{
		// Embed failure info — does NOT contain expected, so test fails
		synthetic = "FAILED: " + to_string(successCount) + "/" +
		            to_string(SEQUENTIAL_COUNT) + " passed. " +
		            "First failure at iteration " + to_string(firstFailAt) + ". " +
		            "Last bad response: " + lastFailResponse.substr(0, 80);
	}

	evaluateStressResult(
		"Rapid Sequential Requests Test",
		"Test to check that the server correctly handles " +
		to_string(SEQUENTIAL_COUNT) + " back-to-back GET requests each on a "
		"new TCP connection — verifying no fd leak and no state corruption "
		"between connections.",
		expected,
		synthetic
	);

	// Diagnosis hint printed regardless of pass/fail
	if (firstFailAt != -1)
	{
		CLI::printHint("First failure at iteration " + to_string(firstFailAt) + " of " + to_string(SEQUENTIAL_COUNT) + ".");
		CLI::printHint("Likely cause: file descriptor leak - server runs out of fds after repeated open/close cycles.");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — Concurrent connections
// Targets: Multiplexer (epoll simultaneous fd handling), ServerManager
//          (accept loop), ServerTask (per-connection state isolation)
//
// Forks CONCURRENT_CLIENTS (10) child processes simultaneously.  Each child
// independently runs one raw GET / request and exits with 0 if it
// receives "HTTP/1.1 200 OK", or with 1 otherwise. Because fork() gives each child
// its own address space and its own socket, there is zero shared state between
// children — avoiding inherited epoll state interactions.
//
// The parent waits for every child with waitpid() and counts how many exited 0.
//
// Why 10? It is large enough to exercise simultaneous accept() handling and
// epoll multi-fd dispatch, but small enough to finish quickly on any machine.
// A server with a single-client epoll bug will fail on the second+ child.
//
// Pass criterion: all CONCURRENT_CLIENTS children exited 0.
// ─────────────────────────────────────────────────────────────────────────────
void StressTests::ConcurrentConnectionsTest()
{
	const string host     = "localhost";
	const string port     = "1025";
	const string request  = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	const string expected = "HTTP/1.1 200 OK";

	cout << endl << "  " << CLI::runBadge() << "  " << CLR_STEP << "Concurrent Connections" << RESET
	     << CLR_DIM << " (" << CONCURRENT_CLIENTS << " simultaneous clients)" << RESET << endl;

	// Fork all children before any wait so they are truly concurrent
	vector<pid_t> pids;
	pids.reserve(CONCURRENT_CLIENTS);

	for (int i = 0; i < CONCURRENT_CLIENTS; i++)
	{
		pid_t pid = fork();
		if (pid < 0)
		{
			cerr << CLR_WARN << "  " << SYM_WARN << " fork() failed at child " << i << RESET << endl;
			// Children already forked must still be reaped below
			break;
		}
		if (pid == 0)
		{
			// ── Child process ───────────────────────────────────────────────
			string response;
			bool ok = childConcurrentRequest(host, port, request, response, RAW_TIMEOUT_MS);
			if (ok && response.find(expected) != string::npos)
				_exit(0);   // success
			else
				_exit(1);   // failure — never reaches parent malloc/destructors
		}
		pids.push_back(pid);
	}

	// ── Parent: reap all children ─────────────────────────────────────────
	int passedChildren = 0;
	int failedChildren = 0;
	for (pid_t pid : pids)
	{
		int status = 0;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			passedChildren++;
		else
			failedChildren++;
	}

	string synthetic;
	if (failedChildren == 0)
	{
		synthetic = expected;
	}
	else
	{
		synthetic = "FAILED: " + to_string(passedChildren) + "/" +
		            to_string((int)pids.size()) + " concurrent clients got 200 OK. " +
		            to_string(failedChildren) + " clients failed.";
	}

	evaluateStressResult(
		"Concurrent Connections Test",
		"Test to check that the server correctly handles " +
		to_string(CONCURRENT_CLIENTS) + " simultaneous TCP connections — "
		"verifying epoll multi-fd dispatch and per-connection state isolation.",
		expected,
		synthetic
	);

	if (failedChildren > 0)
	{
		CLI::printHint(to_string(failedChildren) + " of " + to_string((int)pids.size()) + " concurrent clients did not receive 200 OK.");
		CLI::printHint("Likely cause: server accept() loop does not call accept() until EPOLLIN fires again, or per-connection state is shared.");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — Slow client (byte-at-a-time with delays)
// Targets: ClientRequest (partial read accumulation), Multiplexer (repeated
//          EPOLLIN wakeups for the same fd), ServerTask (request completion
//          detection across multiple recv() calls)
//
// Sends the request string one byte at a time with SLOW_BYTE_DELAY_US (5 ms)
// between each ::send() call.  This forces the server to handle many small
// EPOLLIN events on the same socket fd before the request is complete.
//
// A common bug: the server reads the first recv() chunk, finds no complete
// request header, closes the connection instead of waiting for more data.
// A correct server buffers each partial read and keeps the fd in epoll until
// "\r\n\r\n" is found in the accumulated buffer.
//
// The total send time for "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" (38
// bytes) at 5 ms/byte is 38 × 5 ms = 190 ms — well within the 5000 ms
// timeout set on the recv side.
// ─────────────────────────────────────────────────────────────────────────────
void StressTests::SlowClientTest()
{
	const string host     = "localhost";
	const string port     = "1025";
	const string request  = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	const string expected = "HTTP/1.1 200 OK";

	cout << endl << "  " << CLI::runBadge() << "  " << CLR_STEP << "Slow Client" << RESET
	     << CLR_DIM << " (1 byte per " << SLOW_BYTE_DELAY_US / 1000 << " ms)" << RESET << endl;

	string responseOut;
	bool   gotResponse = false;
	TestCase slowCase;
	slowCase.host = host;
	slowCase.port = port;
	slowCase.timeout = 5000;
	slowCase.socket = -1;
	slowCase.socketIO = NULL;

	if (connectToServer(slowCase))
	{
		// ── Send one byte at a time ───────────────────────────────────────
		bool sendOk = true;
		for (size_t i = 0; i < request.size() && sendOk; i++)
		{
			int n = slowCase.socketIO->Send((void *)(request.c_str() + i), 1);
			if (n != 1)
				sendOk = false;
			else
				usleep(SLOW_BYTE_DELAY_US);
		}

		bool inAdded = false;
		if (sendOk)
		{
			inAdded = multiplexer.AddAsEpollIn(slowCase.socketIO);
			if (inAdded && ReadResponseFromServer(slowCase))
			{
				gotResponse = true;
				responseOut = slowCase.response;
			}
			if (inAdded)
				multiplexer.DeleteFromEpoll(slowCase.socketIO);
		}

		cleanupStressSocket(multiplexer, slowCase);
	}

	string synthetic;
	if (gotResponse && responseOut.find(expected) != string::npos)
		synthetic = expected;
	else if (!gotResponse)
		synthetic = "FAILED: no complete response received. "
		            "Server likely closed the connection before the slow "
		            "client finished sending.";
	else
		synthetic = "FAILED: complete response received but status was not 200 OK. "
		            "Response: " + responseOut.substr(0, 80);

	evaluateStressResult(
		"Slow Client Test",
		"Test to check that the server buffers partial request data across "
		"multiple recv() calls and does not close the connection prematurely "
		"when bytes arrive one at a time with 5 ms delays between them.",
		expected,
		synthetic
	);

	if (!gotResponse)
	{
		CLI::printHint("Server closed the connection before the request was complete.");
		CLI::printHint("Likely cause: ServerTask discards the fd after the first partial recv() instead of keeping it in epoll to accumulate more data.");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — Very large request header value
// Targets: ClientRequest (header size enforcement or acceptance),
//          ServerTask (buffer handling), Repsense (431 builder)
//
// Sends a GET / with a custom header "X-Stress-Header: AAA...8192 bytes".
// The server may legitimately respond with any of:
//   • 200 OK   — if no header-size limit is configured
//   • 431 Request Header Fields Too Large — if the server enforces a limit
//   • 400 Bad Request — also acceptable for a malformed/oversized request
//
// The ONLY unacceptable outcomes are:
//   • No response at all (crash or hang) — connection times out
//   • A response that does not start with "HTTP/1.1" (garbled output)
//
// The expectedResponse is therefore the minimal "HTTP/1.1" prefix — any
// valid HTTP response line satisfies it.  This test proves resilience, not
// a specific status code.
// ─────────────────────────────────────────────────────────────────────────────
void StressTests::LargeRequestHeadersTest()
{
	const string host     = "localhost";
	const string port     = "1025";
	const string expected = "HTTP/1.1";   // any valid response satisfies this

	// Build a request with one enormous custom header value
	string bigValue(LARGE_HEADER_SIZE, 'A');
	string request =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"X-Stress-Header: " + bigValue + "\r\n"
		"\r\n";

	cout << endl << "  " << CLI::runBadge() << "  " << CLR_STEP << "Large Request Header" << RESET
	     << CLR_DIM << " (" << LARGE_HEADER_SIZE << " byte value)" << RESET << endl;

	string response;
	bool   ok = requestWithATestList(host, port, request, response, RAW_TIMEOUT_MS);

	string synthetic;
	if (ok && response.find(expected) != string::npos)
	{
		// Pass — extract first line for the diagnosis printout
		synthetic = response.substr(0, response.find("\r\n") + 2);
	}
	else if (!ok || response.empty())
	{
		synthetic = "FAILED: no response received — server likely crashed or "
		            "hung processing an oversized header.";
	}
	else
	{
		synthetic = "FAILED: response does not start with HTTP/1.1. "
		            "Raw start: " + response.substr(0, 40);
	}

	evaluateStressResult(
		"Large Request Headers Test",
		"Test to check server resilience against a request carrying an "
		"8 KB custom header value — the server must return any valid "
		"HTTP/1.1 status (200, 400, or 431) and must not crash or hang.",
		expected,
		synthetic
	);

	// Print the actual status code received for human review
	if (ok && !response.empty())
	{
		size_t lineEnd = response.find("\r\n");
		string statusLine = (lineEnd != string::npos)
		                    ? response.substr(0, lineEnd)
		                    : response.substr(0, 40);
		CLI::printInfo("Server responded: " + statusLine);
		CLI::printHint("200 = no limit configured; 431 = limit enforced; 400 = rejected as malformed");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — Server remains operational after an error response
// Targets: ServerManager (post-error state reset), Multiplexer (fd cleanup
//          after error path), Repsense (404 builder leaves server clean)
//
// Two sequential requests on independent TCP connections:
//
//   Step 1: GET /this-path-definitely-does-not-exist-stress-test HTTP/1.1
//           → expect 404 Not Found (server must build and send error response)
//
//   Step 2: GET / HTTP/1.1
//           → expect 200 OK  (server must still be alive and clean)
//
// This is the most fundamental resilience test: "your server must remain
// operational at all times" means a 404 on connection A must not corrupt the
// state that handles connection B.
//
// The final assertion is on Step 2 only.  Step 1 is a setup step — if it
// does not produce a 404 that is interesting but not a failure of THIS test.
// The test fails only if Step 2 does not return 200 OK.
// ─────────────────────────────────────────────────────────────────────────────
void StressTests::ConnectionAfterErrorTest()
{
	const string host    = "localhost";
	const string port    = "1025";
	const string expected = "HTTP/1.1 200 OK";

	cout << endl << "  " << CLI::runBadge() << "  " << CLR_STEP << "Connection After Error Response" << RESET << endl;

	// ── Step 1: trigger a 404 ────────────────────────────────────────────────
	string errorRequest =
		"GET /this-path-definitely-does-not-exist-stress-test HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	string errorResponse;
	requestWithATestList(host, port, errorRequest, errorResponse, RAW_TIMEOUT_MS);

	size_t errLine = errorResponse.find("\r\n");
	string errStatus = (errLine != string::npos)
	                   ? errorResponse.substr(0, errLine)
	                   : "(no response)";
	CLI::printInfo("Step 1 (error trigger): " + errStatus);

	// ── Step 2: verify server still alive on a new connection ────────────────
	string okRequest =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	string okResponse;
	bool   ok = requestWithATestList(host, port, okRequest, okResponse, RAW_TIMEOUT_MS);

	string synthetic;
	if (ok && okResponse.find(expected) != string::npos)
	{
		synthetic = expected;
	}
	else if (!ok || okResponse.empty())
	{
		synthetic = "FAILED: no response on the second connection — server "
		            "may have crashed or stopped accepting connections after "
		            "sending the error response on the first connection.";
	}
	else
	{
		synthetic = "FAILED: second connection received something other than "
		            "200 OK. Response: " + okResponse.substr(0, 80);
	}

	evaluateStressResult(
		"Connection After Error Test",
		"Test to check that the server remains fully operational after "
		"sending a 404 error response — a new TCP connection on a valid "
		"path must immediately return 200 OK with no lingering error state.",
		expected,
		synthetic
	);

	if (!ok || okResponse.find(expected) == string::npos)
	{
		CLI::printHint("Step 1 produced: " + errStatus);
		CLI::printHint("Step 2 (should be 200 OK) produced: " + okResponse.substr(0, 80));
		CLI::printHint("Likely cause: the 404 error path left the server fd in a broken state, or a crash occurred during error response construction.");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6 — Keep-alive connection eviction under extreme socket pressure
// Targets: ServerManager / connection pool policy under very high load.
//
// Opens as many keep-alive client sockets as possible (target: 10,000), sends
// one keep-alive request on each, and keeps sockets open. After reaching the
// target (or local process limit), it attempts one additional connection to
// pressure the server's connection limit. Finally, it probes the oldest socket
// to check whether the server closed it.
//
// Note: If the local machine cannot open 10,000 sockets due to ulimit, the test
// reports that as an environment limitation rather than a server failure.
// ─────────────────────────────────────────────────────────────────────────────
void StressTests::KeepAliveConnectionEvictionTest()
{
	const string expectedClosedSignal = "OLDEST_CONNECTION_CLOSED";
	const string keepAliveRequest =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Connection: keep-alive\r\n"
		"\r\n";

	cout << endl << "  " << CLI::runBadge() << "  " << CLR_STEP << "Keep-Alive Eviction" << RESET
	     << CLR_DIM << " (up to " << KEEPALIVE_SWARM << " persistent sockets)" << RESET << endl;

	vector<TestCase *> sockets;
	sockets.reserve(KEEPALIVE_SWARM);

	int opened = 0;
	int openFailures = 0;
	string openFailureReason;

	for (int i = 0; i < KEEPALIVE_SWARM; i++)
	{
		TestCase *conn = new TestCase();
		conn->host = "127.0.0.1";
		conn->port = "1025";
		conn->request = keepAliveRequest;
		conn->timeout = 2000;
		conn->socket = -1;
		conn->socketIO = NULL;

		if (!connectToServer(*conn))
		{
			openFailures++;
			if (openFailureReason.empty())
				openFailureReason = "connect() failed";
			delete conn;
			break;
		}

		if (!SendRequestToServer(*conn))
		{
			openFailures++;
			if (openFailureReason.empty())
				openFailureReason = "send() failed";
			cleanupStressSocket(multiplexer, *conn);
			delete conn;
			break;
		}

		if (!ReadResponseFromServer(*conn))
		{
			openFailures++;
			if (openFailureReason.empty())
				openFailureReason = "read() failed while opening keep-alive socket";
			cleanupStressSocket(multiplexer, *conn);
			delete conn;
			break;
		}

		if (conn->response.find("HTTP/1.1 200 OK") == string::npos)
		{
			openFailures++;
			if (openFailureReason.empty())
				openFailureReason = "keep-alive setup returned non-200 response";
			cleanupStressSocket(multiplexer, *conn);
			delete conn;
			break;
		}

		multiplexer.DeleteFromEpoll(conn->socketIO);
		sockets.push_back(conn);
		opened++;
	}
	
	bool extraConnected = false;
	TestCase extraConn;
	extraConn.socket = -1;
	extraConn.socketIO = NULL;

	if (!sockets.empty())
	{
		extraConn.host = "127.0.0.1";
		extraConn.port = "1025";
		if (connectToServer(extraConn))
		{
			extraConnected = true;
		}
	}

	bool keepAliveSanityOk = false;
	size_t keepAliveSanityIndex = 0;
	if (!sockets.empty())
	{
		// Probe a near-tail socket to verify keep-alive still works on a non-oldest connection.
		keepAliveSanityIndex = (sockets.size() > 20) ? (sockets.size() - 20) : (sockets.size() - 1);
		TestCase *probe = sockets[keepAliveSanityIndex];
		probe->request = keepAliveRequest;
		probe->sendedBytes = 0;
		probe->response.clear();
		probe->contentLength = (size_t)-1;
		probe->headerLength = (size_t)-1;
		if (SendRequestToServer(*probe)
			&& ReadResponseFromServer(*probe)
			&& probe->response.find("HTTP/1.1 200 OK") != string::npos)
		{
			keepAliveSanityOk = true;
		}
		multiplexer.DeleteFromEpoll(probe->socketIO);
	}

	bool oldestClosed = false;
	if (!sockets.empty())
	{
		TestCase *oldest = sockets.front();
		oldest->request = keepAliveRequest;
		oldest->sendedBytes = 0;
		oldest->response.clear();
		oldest->contentLength = (size_t)-1;
		oldest->headerLength = (size_t)-1;
		if (!SendRequestToServer(*oldest))
		{
			oldestClosed = true;
		}
		else
		{
			if (!ReadResponseFromServer(*oldest) || oldest->response.empty())
				oldestClosed = true;
		}
		multiplexer.DeleteFromEpoll(oldest->socketIO);
	}

	if (extraConn.socketIO)
		cleanupStressSocket(multiplexer, extraConn);
	for (size_t i = 0; i < sockets.size(); i++)
	{
		cleanupStressSocket(multiplexer, *sockets[i]);
		delete sockets[i];
	}

	string synthetic;
	if (opened < KEEPALIVE_SWARM)
	{
		synthetic = "FAILED: opened " + to_string(opened) + "/" + to_string(KEEPALIVE_SWARM) +
		            " sockets before failure (" + openFailureReason + "). "
		            "Likely local ulimit or environment ceiling.";
	}
	else if (!keepAliveSanityOk)
	{
		synthetic = "FAILED: keep-alive sanity probe failed on socket index " +
		            to_string(keepAliveSanityIndex) + ". "
		            "Could not confirm that non-oldest keep-alive connections still work.";
	}
	else if (oldestClosed)
	{
		synthetic = expectedClosedSignal;
	}
	else
	{
		synthetic = "FAILED: oldest keep-alive socket remained usable after 10,000 sockets. "
		            "Server did not evict oldest connection under pressure. "
		            "Extra connection " + string(extraConnected ? "succeeded" : "failed") + ".";
	}

	evaluateStressResult(
		"Keep-Alive Connection Eviction Test",
		"Test to open 10,000 keep-alive sockets and verify whether the server "
		"starts closing the oldest persistent connection when capacity is under "
		"extreme pressure.",
		expectedClosedSignal,
		synthetic
	);

	CLI::printInfo("Opened sockets: " + to_string(opened) + "/" + to_string(KEEPALIVE_SWARM));
	if (opened < KEEPALIVE_SWARM)
		CLI::printHint("Could not reach 10,000 sockets. Check process/server ulimit (nofile). " + openFailureReason);
	else if (!keepAliveSanityOk)
		CLI::printHint("Keep-alive sanity probe on socket index " + to_string(keepAliveSanityIndex) + " failed. Verify persistent-connection handling before eviction assertions.");
	else if (!oldestClosed)
		CLI::printHint("Oldest keep-alive connection stayed open. If eviction is expected, inspect your LRU/idle connection cleanup policy.");
}

// ─────────────────────────────────────────────────────────────────────────────
void StressTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Rapid Sequential Requests Test", (void (ATestList::*)())&StressTests::RapidSequentialRequestsTest) );
	_testFunctions.push_back( make_pair("Concurrent Connections Test",    (void (ATestList::*)())&StressTests::ConcurrentConnectionsTest) );
	_testFunctions.push_back( make_pair("Slow Client Test",               (void (ATestList::*)())&StressTests::SlowClientTest) );
	_testFunctions.push_back( make_pair("Large Request Headers Test",     (void (ATestList::*)())&StressTests::LargeRequestHeadersTest) );
	_testFunctions.push_back( make_pair("Connection After Error Test",    (void (ATestList::*)())&StressTests::ConnectionAfterErrorTest) );
	_testFunctions.push_back( make_pair("Keep-Alive Connection Eviction Test", (void (ATestList::*)())&StressTests::KeepAliveConnectionEvictionTest) );
}
