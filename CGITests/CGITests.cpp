#include "CGITests.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// SERVER CONFIG AND FILE SETUP — required before running this suite
//
// Config block (add to your server config):
//
//   location /cgi-bin {
//       root       <your-web-root>/cgi-bin;
//       cgi_ext    .py;                    # or however your server maps extensions
//       index      hello.py;
//   }
//
// All scripts must be created inside <web-root>/cgi-bin/ and made executable
// with chmod +x.  Exact shell commands are provided in each test's
// configFileData.
//
// ── CRITICAL: Content-Length in EVERY response ───────────────────────────────
// ReadResponseFromServer terminates only when it has received
//     header_length + content_length   bytes.
// If the server sends a CGI response without Content-Length, the test reader
// loops forever waiting for more data and the test times out with a failure.
//
// Two cases:
//   a) CGI script outputs Content-Length itself → server must forward it.
//   b) CGI script omits Content-Length         → server MUST buffer all CGI
//      stdout until EOF, compute the body size, and inject Content-Length.
//      (Test 4 specifically validates this behaviour.)
//
// ── CGI response format ───────────────────────────────────────────────────────
// A CGI script writes to stdout in this format (using \n line endings):
//
//   Content-Type: text/plain\n
//   Content-Length: N\n          ← optional; server must inject if missing
//   \n
//   <body>
//
// The server prepends "HTTP/1.1 200 OK\r\n" and forwards the headers and body
// to the client.
// ─────────────────────────────────────────────────────────────────────────────

CGITests::CGITests() : ATestList("CGI Tests")
{
	AddAllTests();
}

CGITests::~CGITests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — Basic CGI GET execution
// Targets: CGI (fork/exec), CGIPipe (stdout capture), AMethod (GET dispatch),
//          Routing (extension-based CGI detection)
//
// The most fundamental CGI test — proves the full execution pipeline:
// 1. Server detects .py extension and routes to CGI handler.
// 2. Server forks and execs the Python interpreter.
// 3. Server reads CGI stdout and forwards it as the HTTP response body.
// 4. Response contains the sentinel the script printed.
//
// The sentinel 'CGI-GET-SENTINEL' exists only in the CGI output — it will
// never appear in a static file response or an error page.
//
// Script (save as <web-root>/cgi-bin/hello.py, chmod +x):
//   #!/usr/bin/env python3
//   body = "CGI-GET-SENTINEL"
//   print("Content-Type: text/plain")
//   print("Content-Length: " + str(len(body)))
//   print()
//   print(body, end="")
// ─────────────────────────────────────────────────────────────────────────────
void CGITests::CgiGetRequestTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "CGI Get Request Test";
	testCase.description = "Test to check the basic CGI execution pipeline: "
	                       "server detects .py extension, forks and execs the "
	                       "interpreter, captures stdout, and forwards the body "
	                       "to the client.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /cgi-bin/hello.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Sentinel printed by the script — only appears in CGI output
	testCase.expectedResponse = "CGI-GET-SENTINEL";

	testCase.configFileData =
		"SETUP: Create <web-root>/cgi-bin/hello.py with:\n"
		"  #!/usr/bin/env python3\n"
		"  body = \"CGI-GET-SENTINEL\"\n"
		"  print(\"Content-Type: text/plain\")\n"
		"  print(\"Content-Length: \" + str(len(body)))\n"
		"  print()\n"
		"  print(body, end=\"\")\n"
		"Then: chmod +x <web-root>/cgi-bin/hello.py\n"
		"Config: add 'cgi_ext .py' (or equivalent) to the /cgi-bin location.\n"
		"If this returns 404, the extension-to-CGI mapping is not configured.\n"
		"If it returns 200 but the sentinel is missing, CGI stdout is not being "
		"forwarded to the client — check the CGIPipe read loop.";

	// CGI tests need a larger timeout: process fork + exec + Python startup
	testCase.timeout = 5000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — CGI POST body piped to stdin
// Targets: CGI (stdin pipe setup), CGIPipe, AMethod (POST dispatch),
//          SessionManagement (body forwarding)
//
// Sends a POST request with a known body. The CGI script reads exactly
// CONTENT_LENGTH bytes from stdin and echoes them back in the response.
// A match on the echoed string proves the server:
//   1. Set up the stdin pipe correctly (CGIPipe).
//   2. Wrote the full request body into the pipe.
//   3. Set CONTENT_LENGTH in the CGI environment.
//
// Script (save as <web-root>/cgi-bin/echo_post.py, chmod +x):
//   #!/usr/bin/env python3
//   import os, sys
//   length = int(os.environ.get("CONTENT_LENGTH", 0))
//   body = sys.stdin.read(length) if length > 0 else ""
//   print("Content-Type: text/plain")
//   print("Content-Length: " + str(len(body)))
//   print()
//   print(body, end="")
// ─────────────────────────────────────────────────────────────────────────────
void CGITests::CgiPostWithBodyTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "CGI Post With Body Test";
	testCase.description = "Test to check that the server correctly pipes the "
	                       "POST request body to the CGI script's stdin — the "
	                       "script echoes it back and a match proves end-to-end "
	                       "stdin delivery.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// The script reads and echoes this exact string from stdin
	string body          = "CGI-POST-INPUT";
	string contentLength = to_string(body.size());

	testCase.request =
		"POST /cgi-bin/echo_post.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	// The echoed string must appear in the response body
	testCase.expectedResponse = "CGI-POST-INPUT";

	testCase.configFileData =
		"SETUP: Create <web-root>/cgi-bin/echo_post.py with:\n"
		"  #!/usr/bin/env python3\n"
		"  import os, sys\n"
		"  length = int(os.environ.get(\"CONTENT_LENGTH\", 0))\n"
		"  body = sys.stdin.read(length) if length > 0 else \"\"\n"
		"  print(\"Content-Type: text/plain\")\n"
		"  print(\"Content-Length: \" + str(len(body)))\n"
		"  print()\n"
		"  print(body, end=\"\")\n"
		"Then: chmod +x <web-root>/cgi-bin/echo_post.py\n"
		"If the test returns the correct status but the body is empty, "
		"the server is not writing the request body into the stdin pipe "
		"(check the CGIPipe send loop). "
		"If CONTENT_LENGTH is missing from the environment the script reads "
		"0 bytes and echoes an empty string — check env var population.";

	testCase.timeout = 5000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — CGI environment variable: REQUEST_METHOD
// Targets: CGI (env var population), CGIRequest (env construction),
//          SessionManagement
//
// The script prints the value of REQUEST_METHOD to its output. The test
// asserts the string "REQUEST_METHOD=GET" appears in the response body.
// This proves the server populated at least the most fundamental env var.
//
// Testing with the full "REQUEST_METHOD=GET" string (not just "GET") avoids
// a false positive where the method name happens to appear in a header or
// error message unrelated to CGI env vars.
//
// Script (save as <web-root>/cgi-bin/env_check.py, chmod +x):
//   #!/usr/bin/env python3
//   import os
//   line = "REQUEST_METHOD=" + os.environ.get("REQUEST_METHOD", "MISSING")
//   print("Content-Type: text/plain")
//   print("Content-Length: " + str(len(line)))
//   print()
//   print(line, end="")
// ─────────────────────────────────────────────────────────────────────────────
void CGITests::CgiEnvVarsTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "CGI Env Vars Test";
	testCase.description = "Test to check that the server populates the CGI "
	                       "environment correctly — the script prints "
	                       "'REQUEST_METHOD=GET' and a match confirms the env "
	                       "var was set before exec().";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /cgi-bin/env_check.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Script outputs "REQUEST_METHOD=GET" — full key=value match avoids false positives
	testCase.expectedResponse = "REQUEST_METHOD=GET";

	testCase.configFileData =
		"SETUP: Create <web-root>/cgi-bin/env_check.py with:\n"
		"  #!/usr/bin/env python3\n"
		"  import os\n"
		"  line = \"REQUEST_METHOD=\" + os.environ.get(\"REQUEST_METHOD\", \"MISSING\")\n"
		"  print(\"Content-Type: text/plain\")\n"
		"  print(\"Content-Length: \" + str(len(line)))\n"
		"  print()\n"
		"  print(line, end=\"\")\n"
		"Then: chmod +x <web-root>/cgi-bin/env_check.py\n"
		"If the body contains 'REQUEST_METHOD=MISSING', the CGIRequest env "
		"builder is not setting REQUEST_METHOD before execve(). "
		"Other required env vars to verify manually: SERVER_PROTOCOL, "
		"PATH_INFO, SCRIPT_NAME, SERVER_NAME, SERVER_PORT.";

	testCase.timeout = 5000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — Server injects Content-Length when CGI omits it
// Targets: CGI (stdout collection until EOF), CGIPipe (EOF detection),
//          Repsense (Content-Length injection)
//
// The subject states: "If no content_length is returned from the CGI, EOF
// will mark the end of the returned data."
// This test proves the server honours that requirement:
//   1. CGI script outputs ONLY Content-Type header — no Content-Length.
//   2. Server reads CGI stdout until the pipe produces EOF.
//   3. Server buffers the entire body, computes its byte count.
//   4. Server injects "Content-Length: N" into the HTTP response.
//
// Without step 4, ReadResponseFromServer loops forever because it never
// finds Content-Length in the response, causing a timeout.
//
// The expectedResponse is "Content-Length:" — a substring that only appears
// if the server injected it, since the CGI script deliberately omits it.
//
// Script (save as <web-root>/cgi-bin/no_cl.py, chmod +x):
//   #!/usr/bin/env python3
//   body = "CGI-NO-CL-BODY"
//   print("Content-Type: text/plain")
//   print()
//   print(body, end="")
// ─────────────────────────────────────────────────────────────────────────────
void CGITests::CgiNoContentLengthTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "CGI No Content Length Test";
	testCase.description = "Test to check that when a CGI script does not output "
	                       "a Content-Length header, the server buffers all CGI "
	                       "stdout until EOF and injects the correct Content-Length "
	                       "into the HTTP response itself.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /cgi-bin/no_cl.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Proves the server injected Content-Length — the script never outputs it
	testCase.expectedResponse = "Content-Length:";

	testCase.configFileData =
		"SETUP: Create <web-root>/cgi-bin/no_cl.py with:\n"
		"  #!/usr/bin/env python3\n"
		"  body = \"CGI-NO-CL-BODY\"\n"
		"  print(\"Content-Type: text/plain\")\n"
		"  print()\n"
		"  print(body, end=\"\")\n"
		"Then: chmod +x <web-root>/cgi-bin/no_cl.py\n"
		"IMPORTANT: The script deliberately has NO 'Content-Length' print line. "
		"The server must detect EOF on the CGI stdout pipe, collect all output, "
		"measure the body size, and write 'Content-Length: 14' (or the actual "
		"byte count) into the HTTP response headers. "
		"If this test times out, the server is not injecting Content-Length — "
		"ReadResponseFromServer loops forever waiting for it. "
		"If it returns a 200 but 'Content-Length:' is not found in the response, "
		"the injection code path is skipped when CGI omits the header.";

	testCase.timeout = 5000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — QUERY_STRING environment variable
// Targets: CGIRequest (env var construction), Path (query string extraction),
//          ClientRequest (raw URL preservation)
//
// The query string from the request URL must reach the CGI as the QUERY_STRING
// environment variable, without the leading '?' character and without any
// percent-decoding (QUERY_STRING is passed raw per CGI/1.1 RFC 3875 §4.1.7).
//
// The script prints the value of QUERY_STRING. Asserting on "color=blue" (a
// substring of the full query string "color=blue&size=large") avoids any
// ordering dependency if the server re-encodes or reorders parameters.
//
// Script (save as <web-root>/cgi-bin/query.py, chmod +x):
//   #!/usr/bin/env python3
//   import os
//   qs = os.environ.get("QUERY_STRING", "MISSING")
//   print("Content-Type: text/plain")
//   print("Content-Length: " + str(len(qs)))
//   print()
//   print(qs, end="")
// ─────────────────────────────────────────────────────────────────────────────
void CGITests::CgiQueryStringTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "CGI Query String Test";
	testCase.description = "Test to check that the server correctly extracts the "
	                       "query string from the request URL and passes it to "
	                       "the CGI as the QUERY_STRING environment variable "
	                       "(without the leading '?' character).";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Query string: color=blue&size=large
	testCase.request =
		"GET /cgi-bin/query.py?color=blue&size=large HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Substring match — avoids ordering issues; "color=blue" must be present
	testCase.expectedResponse = "color=blue";

	testCase.configFileData =
		"SETUP: Create <web-root>/cgi-bin/query.py with:\n"
		"  #!/usr/bin/env python3\n"
		"  import os\n"
		"  qs = os.environ.get(\"QUERY_STRING\", \"MISSING\")\n"
		"  print(\"Content-Type: text/plain\")\n"
		"  print(\"Content-Length: \" + str(len(qs)))\n"
		"  print()\n"
		"  print(qs, end=\"\")\n"
		"Then: chmod +x <web-root>/cgi-bin/query.py\n"
		"Expected body: 'color=blue&size=large' (no leading '?'). "
		"If the body contains 'MISSING', QUERY_STRING is not being set. "
		"If the body contains '?color=blue', the leading '?' was not stripped — "
		"CGI/1.1 §4.1.7 requires QUERY_STRING to NOT include the '?'. "
		"Remember: QUERY_STRING is passed raw (no percent-decoding).";

	testCase.timeout = 5000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6 — CGI working directory for relative file access
// Targets: CGI (chdir before exec), CGIRequest (working dir setup)
//
// The subject requires: "The CGI should be run in the correct directory for
// relative path file access."
//
// The script opens "data.txt" using ONLY the bare filename — no path prefix.
// This only succeeds if the server has chdir()'d to the script's own directory
// before calling execve(). The file data.txt sits in the same directory as the
// script and contains the sentinel string.
//
// If the server does NOT chdir, the open() call fails (FileNotFoundError), the
// script exits with a non-zero status, and the server returns 500 or an error.
//
// Script (save as <web-root>/cgi-bin/dir_check.py, chmod +x):
//   #!/usr/bin/env python3
//   with open("data.txt", "r") as f:
//       body = f.read().strip()
//   print("Content-Type: text/plain")
//   print("Content-Length: " + str(len(body)))
//   print()
//   print(body, end="")
//
// Data file (save as <web-root>/cgi-bin/data.txt):
//   CGI-WORKDIR-SENTINEL
// ─────────────────────────────────────────────────────────────────────────────
void CGITests::CgiWorkingDirectoryTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "CGI Working Directory Test";
	testCase.description = "Test to check that the server sets the CGI working "
	                       "directory to the script's own directory before exec() "
	                       "— the script opens 'data.txt' by bare filename only, "
	                       "which requires chdir() to the script's directory first.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /cgi-bin/dir_check.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Sentinel lives in data.txt beside the script — only readable if chdir() worked
	testCase.expectedResponse = "CGI-WORKDIR-SENTINEL";

	testCase.configFileData =
		"SETUP:\n"
		"1. Create <web-root>/cgi-bin/dir_check.py with:\n"
		"     #!/usr/bin/env python3\n"
		"     with open(\"data.txt\", \"r\") as f:\n"
		"         body = f.read().strip()\n"
		"     print(\"Content-Type: text/plain\")\n"
		"     print(\"Content-Length: \" + str(len(body)))\n"
		"     print()\n"
		"     print(body, end=\"\")\n"
		"   chmod +x <web-root>/cgi-bin/dir_check.py\n"
		"2. Create <web-root>/cgi-bin/data.txt with content: CGI-WORKDIR-SENTINEL\n"
		"   echo 'CGI-WORKDIR-SENTINEL' > <web-root>/cgi-bin/data.txt\n"
		"If this returns 500 or an empty body, the server did not call chdir() to "
		"the script's directory before execve() — the open(\"data.txt\") fails "
		"because the process CWD is still the server's root. "
		"Fix: in your CGI fork, call chdir(script_directory) after fork() and "
		"before execve().";

	testCase.timeout = 5000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 7 — CGI process timeout → 504 Gateway Timeout
// Targets: CGI (watchdog / SIGKILL), CGIPipe (timeout detection),
//          Multiplexer (epoll timeout on CGI pipe fd), Repsense (504 builder)
//
// A CGI script that sleeps for 30 seconds must be killed by the server's
// CGI watchdog before the test's own timeout fires.  The server must then
// return 504 Gateway Timeout with a body and Content-Length.
//
// Timeout relationship:
//   Script sleep  = 30 s
//   Server CGI timeout (configure in your server) = 5 s  (recommended)
//   This test's timeout                           = 8000 ms
//
// The server must kill the CGI process within its own timeout, then send the
// 504 response.  Since 5 s < 8 s, the test reader will receive the 504 before
// its own epollWait fires.  If the server's CGI timeout is longer than 8 s,
// this test itself will time out with a "Timeout while waiting for EpollIn"
// error — in that case, reduce the server's CGI timeout.
//
// Script (save as <web-root>/cgi-bin/sleeper.py, chmod +x):
//   #!/usr/bin/env python3
//   import time
//   time.sleep(30)
//   print("Content-Type: text/plain")
//   print("Content-Length: 0")
//   print()
// ─────────────────────────────────────────────────────────────────────────────
void CGITests::CgiTimeoutTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "CGI Timeout Test";
	testCase.description = "Test to check that when a CGI script hangs (sleeps "
	                       "for 30 s), the server detects the timeout, kills the "
	                       "CGI process, and returns 504 Gateway Timeout before "
	                       "this test's own 8-second deadline expires.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /cgi-bin/sleeper.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse = "HTTP/1.1 504 Gateway Timeout";

	testCase.configFileData =
		"SETUP: Create <web-root>/cgi-bin/sleeper.py with:\n"
		"  #!/usr/bin/env python3\n"
		"  import time\n"
		"  time.sleep(30)\n"
		"  print(\"Content-Type: text/plain\")\n"
		"  print(\"Content-Length: 0\")\n"
		"  print()\n"
		"Then: chmod +x <web-root>/cgi-bin/sleeper.py\n"
		"TIMEOUT SETUP: Configure your server's CGI timeout to 5 seconds (or any "
		"value under 8000 ms). The relationship must be:\n"
		"  script sleep (30 s) > server CGI timeout (~5 s) > this test timeout (8 s)? NO:\n"
		"  server CGI timeout (~5 s) < this test's timeout (8 s) < script sleep (30 s).\n"
		"If this test times out with 'Timeout while waiting for EpollIn events', "
		"the server's CGI watchdog is not firing — check that the CGIPipe fd is "
		"monitored by epoll and that a timeout triggers SIGKILL on the child PID. "
		"CRITICAL: The 504 response MUST include Content-Length or this test's "
		"reader will block even after the 504 is sent.";

	// 8000 ms — server CGI timeout must be shorter than this
	testCase.timeout = 8000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void CGITests::AddAllTests()
{
	_testFunctions.push_back( make_pair("CGI Get Request Test",          (void (ATestList::*)())&CGITests::CgiGetRequestTest) );
	_testFunctions.push_back( make_pair("CGI Post With Body Test",       (void (ATestList::*)())&CGITests::CgiPostWithBodyTest) );
	_testFunctions.push_back( make_pair("CGI Env Vars Test",             (void (ATestList::*)())&CGITests::CgiEnvVarsTest) );
	_testFunctions.push_back( make_pair("CGI No Content Length Test",    (void (ATestList::*)())&CGITests::CgiNoContentLengthTest) );
	_testFunctions.push_back( make_pair("CGI Query String Test",         (void (ATestList::*)())&CGITests::CgiQueryStringTest) );
	_testFunctions.push_back( make_pair("CGI Working Directory Test",    (void (ATestList::*)())&CGITests::CgiWorkingDirectoryTest) );
	_testFunctions.push_back( make_pair("CGI Timeout Test",              (void (ATestList::*)())&CGITests::CgiTimeoutTest) );
}
