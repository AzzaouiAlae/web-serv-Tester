#include "StressTests.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// WHY STRESS TESTS BYPASS THE ATESTLIST INFRASTRUCTURE
//
// ATestList owns one Multiplexer (epoll) instance.  All three protected
// methods — connectToServer, SendRequestToServer, ReadResponseFromServer —
// register and deregister file descriptors on that single epoll instance.
// This is safe for sequential use but becomes a race when multiple sockets
// are live at the same time.
//
// Stress tests that need concurrent or rapid-fire connections therefore use:
//   rawRequest() — a self-contained POSIX socket function (socket / connect /
//                  send / recv) with SO_RCVTIMEO for timeout.
//   fork()       — each child process gets its own address space, its own
//                  socket, and cannot interfere with the parent's multiplexer.
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
static const int    SEQUENTIAL_COUNT    = 50;    // requests in Test 1
static const int    CONCURRENT_CLIENTS  = 10;    // forked children in Test 2
static const int    SLOW_BYTE_DELAY_US  = 5000;  // 5 ms between bytes in Test 3
static const size_t LARGE_HEADER_SIZE   = 8192;  // bytes in custom header Test 4
static const int    RAW_TIMEOUT_MS      = 3000;  // SO_RCVTIMEO for rawRequest()

// ─────────────────────────────────────────────────────────────────────────────
// rawRequest — static helper
//
// Opens a blocking TCP socket to host:port, sends the full request string,
// then reads until both conditions are true:
//   • The header block ending "\r\n\r\n" has been received.
//   • response.size() >= headerLength + contentLength
//   (Mirrors the termination logic in ATestList::ReadResponseFromServer.)
//
// A SO_RCVTIMEO of timeoutMs milliseconds is set before connect() so that
// a non-responsive server does not block the caller indefinitely.
//
// Safe to call from forked child processes and from std::thread contexts
// because it allocates no shared state.
// ─────────────────────────────────────────────────────────────────────────────
bool StressTests::rawRequest(const string &host, const string &port,
                             const string &request, string &responseOut,
                             int timeoutMs)
{
	responseOut.clear();

	// ── Open socket ──────────────────────────────────────────────────────────
	int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return false;

	// ── Set receive timeout ──────────────────────────────────────────────────
	struct timeval tv;
	tv.tv_sec  = timeoutMs / 1000;
	tv.tv_usec = (timeoutMs % 1000) * 1000;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	// ── Connect ──────────────────────────────────────────────────────────────
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port   = htons((uint16_t)stoi(port));
	const char *ip  = (host == "localhost") ? "127.0.0.1" : host.c_str();
	if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0 ||
	    ::connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		::close(fd);
		return false;
	}

	// ── Send full request ────────────────────────────────────────────────────
	size_t sent = 0;
	while (sent < request.size())
	{
		int n = ::send(fd, request.c_str() + sent, request.size() - sent, 0);
		if (n <= 0)
		{
			::close(fd);
			return false;
		}
		sent += (size_t)n;
	}

	// ── Read response — same Content-Length termination as ATestList ─────────
	char buf[1024];
	while (true)
	{
		int n = ::recv(fd, buf, sizeof(buf), 0);
		if (n <= 0)
			break;   // timeout or connection closed — use whatever we have
		responseOut.append(buf, (size_t)n);

		// Look for end-of-headers marker
		size_t headerEnd = responseOut.find("\r\n\r\n");
		if (headerEnd == string::npos)
			continue;
		size_t headerLength = headerEnd + 4;

		// Extract Content-Length value
		size_t clPos = responseOut.find("Content-Length:");
		if (clPos == string::npos)
			continue;
		clPos += strlen("Content-Length:");
		while (clPos < responseOut.size() && isspace((unsigned char)responseOut[clPos]))
			clPos++;
		size_t clEnd = responseOut.find("\r\n", clPos);
		if (clEnd == string::npos)
			continue;
		size_t contentLength = (size_t)atoi(responseOut.substr(clPos, clEnd - clPos).c_str());

		if (responseOut.size() >= headerLength + contentLength)
			break;   // complete response received
	}

	::close(fd);
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
	result.expectedResponse = expectedResponse;
	result.response         = syntheticResponse;
	// Set headerLength so actServerResponse can call substr(0, headerLength)
	// without reading past the synthetic string.
	result.headerLength     = min(syntheticResponse.size(),
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
// Sends SEQUENTIAL_COUNT (50) GET / requests back-to-back, each on its own
// new TCP connection, using rawRequest() so the parent multiplexer is never
// touched.  A fresh connection is opened and closed for every iteration.
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

	cout << "Stress Test 1 — Rapid Sequential Requests ("
	     << SEQUENTIAL_COUNT << " iterations)..." << endl;

	int  successCount = 0;
	int  firstFailAt  = -1;
	string lastFailResponse;

	for (int i = 0; i < SEQUENTIAL_COUNT; i++)
	{
		string response;
		bool ok = rawRequest(host, port, request, response, RAW_TIMEOUT_MS);

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
		synthetic = expected + "\r\nContent-Length: 0\r\n\r\n";
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
		cerr << "  First failure at iteration " << firstFailAt
		     << " of " << SEQUENTIAL_COUNT << "." << endl;
		cerr << "  Likely cause: file descriptor leak — server runs out of fds "
		        "after repeated open/close cycles." << endl;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — Concurrent connections
// Targets: Multiplexer (epoll simultaneous fd handling), ServerManager
//          (accept loop), ServerTask (per-connection state isolation)
//
// Forks CONCURRENT_CLIENTS (10) child processes simultaneously.  Each child
// independently calls rawRequest() for GET / and exits with 0 if it receives
// "HTTP/1.1 200 OK", or with 1 otherwise.  Because fork() gives each child
// its own address space and its own socket, there is zero shared state between
// children — the parent's multiplexer is untouched.
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

	cout << "Stress Test 2 — Concurrent Connections ("
	     << CONCURRENT_CLIENTS << " simultaneous clients)..." << endl;

	// Fork all children before any wait so they are truly concurrent
	vector<pid_t> pids;
	pids.reserve(CONCURRENT_CLIENTS);

	for (int i = 0; i < CONCURRENT_CLIENTS; i++)
	{
		pid_t pid = fork();
		if (pid < 0)
		{
			cerr << "fork() failed at child " << i << endl;
			// Children already forked must still be reaped below
			break;
		}
		if (pid == 0)
		{
			// ── Child process ───────────────────────────────────────────────
			string response;
			bool ok = rawRequest(host, port, request, response, RAW_TIMEOUT_MS);
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
		synthetic = expected + "\r\nContent-Length: 0\r\n\r\n";
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
		cerr << "  " << failedChildren << " of " << pids.size()
		     << " concurrent clients did not receive 200 OK." << endl;
		cerr << "  Likely cause: server accept() loop does not call accept() "
		        "until EPOLLIN fires again, or per-connection state is shared." << endl;
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

	cout << "Stress Test 3 — Slow Client (1 byte per "
	     << SLOW_BYTE_DELAY_US / 1000 << " ms)..." << endl;

	string responseOut;
	bool   gotResponse = false;

	// ── Open raw socket ───────────────────────────────────────────────────────
	int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd >= 0)
	{
		// 5-second receive timeout — slow send takes ~190 ms, plenty of margin
		struct timeval tv;
		tv.tv_sec  = 5;
		tv.tv_usec = 0;
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port   = htons(1025);
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

		if (::connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
		{
			// ── Send one byte at a time ───────────────────────────────────────
			bool sendOk = true;
			for (size_t i = 0; i < request.size() && sendOk; i++)
			{
				int n = ::send(fd, request.c_str() + i, 1, 0);
				if (n != 1)
					sendOk = false;
				else
					usleep(SLOW_BYTE_DELAY_US);
			}

			if (sendOk)
			{
				// ── Read complete response ─────────────────────────────────────
				char buf[1024];
				while (true)
				{
					int n = ::recv(fd, buf, sizeof(buf), 0);
					if (n <= 0)
						break;
					responseOut.append(buf, (size_t)n);

					size_t headerEnd = responseOut.find("\r\n\r\n");
					if (headerEnd == string::npos)
						continue;
					size_t headerLength = headerEnd + 4;

					size_t clPos = responseOut.find("Content-Length:");
					if (clPos == string::npos)
						continue;
					clPos += strlen("Content-Length:");
					while (clPos < responseOut.size() && isspace((unsigned char)responseOut[clPos]))
						clPos++;
					size_t clEnd = responseOut.find("\r\n", clPos);
					if (clEnd == string::npos)
						continue;
					size_t contentLength = (size_t)atoi(responseOut.substr(clPos, clEnd - clPos).c_str());

					if (responseOut.size() >= headerLength + contentLength)
					{
						gotResponse = true;
						break;
					}
				}
			}
		}
		::close(fd);
	}

	string synthetic;
	if (gotResponse && responseOut.find(expected) != string::npos)
		synthetic = expected + "\r\nContent-Length: 0\r\n\r\n";
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
		cerr << "  Server closed the connection before the request was complete." << endl;
		cerr << "  Likely cause: ServerTask discards the fd after the first "
		        "partial recv() instead of keeping it in epoll to accumulate "
		        "more data." << endl;
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

	cout << "Stress Test 4 — Large Request Header ("
	     << LARGE_HEADER_SIZE << " byte value)..." << endl;

	string response;
	bool   ok = rawRequest(host, port, request, response, RAW_TIMEOUT_MS);

	string synthetic;
	if (ok && response.find(expected) != string::npos)
	{
		// Pass — extract first line for the diagnosis printout
		synthetic = response.substr(0, response.find("\r\n") + 2) +
		            "Content-Length: 0\r\n\r\n";
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
		cout << "  Server responded: " << statusLine << endl;
		cout << "  (200 = no limit configured; 431 = limit enforced; "
		        "400 = rejected as malformed)" << endl;
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

	cout << "Stress Test 5 — Connection After Error Response..." << endl;

	// ── Step 1: trigger a 404 ────────────────────────────────────────────────
	string errorRequest =
		"GET /this-path-definitely-does-not-exist-stress-test HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	string errorResponse;
	rawRequest(host, port, errorRequest, errorResponse, RAW_TIMEOUT_MS);

	size_t errLine = errorResponse.find("\r\n");
	string errStatus = (errLine != string::npos)
	                   ? errorResponse.substr(0, errLine)
	                   : "(no response)";
	cout << "  Step 1 (error trigger): " << errStatus << endl;

	// ── Step 2: verify server still alive on a new connection ────────────────
	string okRequest =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	string okResponse;
	bool   ok = rawRequest(host, port, okRequest, okResponse, RAW_TIMEOUT_MS);

	string synthetic;
	if (ok && okResponse.find(expected) != string::npos)
	{
		synthetic = expected + "\r\nContent-Length: 0\r\n\r\n";
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
		cerr << "  Step 1 produced: " << errStatus << endl;
		cerr << "  Step 2 (should be 200 OK) produced: "
		     << okResponse.substr(0, 80) << endl;
		cerr << "  Likely cause: the 404 error path left the server fd in a "
		        "broken state, or a crash occurred during error response "
		        "construction." << endl;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
void StressTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Rapid Sequential Requests Test", (void (ATestList::*)())&StressTests::RapidSequentialRequestsTest) );
	_testFunctions.push_back( make_pair("Concurrent Connections Test",    (void (ATestList::*)())&StressTests::ConcurrentConnectionsTest) );
	_testFunctions.push_back( make_pair("Slow Client Test",               (void (ATestList::*)())&StressTests::SlowClientTest) );
	_testFunctions.push_back( make_pair("Large Request Headers Test",     (void (ATestList::*)())&StressTests::LargeRequestHeadersTest) );
	_testFunctions.push_back( make_pair("Connection After Error Test",    (void (ATestList::*)())&StressTests::ConnectionAfterErrorTest) );
}
