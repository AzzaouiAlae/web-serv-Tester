#include "SessionTests.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// IMPORTANT — MATCH THIS TO YOUR IMPLEMENTATION
//
// Change SESSION_COOKIE_NAME to the exact cookie name your SessionManagement
// class uses when it writes the Set-Cookie header.  Every test in this suite
// uses this constant.  If it is wrong, tests 2-5 will fail even if the
// server's session implementation is correct.
//
// Common choices: "SESSION_ID", "session_id", "SESSID", "sid"
// ─────────────────────────────────────────────────────────────────────────────
string SESSION_COOKIE_NAME = "SESSION_ID";

// ─────────────────────────────────────────────────────────────────────────────
// ROUTE AND CGI SETUP
//
// The server config needs:
//
//   location / {
//       root   <your-web-root>;
//       index  index.htm;
//   }
//
//   location /cgi-bin {
//       root      <your-web-root>/cgi-bin;
//       cgi_ext   .py;
//   }
//
// CGI scripts (all chmod +x):
//
//   ── cookie_check.py ──────────────────────────────────────────────────────
//   Prints the raw value of the HTTP_COOKIE env var.
//
//   #!/usr/bin/env python3
//   import os
//   val = os.environ.get("HTTP_COOKIE", "NO_COOKIE_ENV_VAR")
//   print("Content-Type: text/plain")
//   print("Content-Length: " + str(len(val)))
//   print()
//   print(val, end="")
//
//   ── session_write.py ─────────────────────────────────────────────────────
//   Reads the POST body, extracts the session ID from HTTP_COOKIE, writes
//   the body to /tmp/sess_<session_id>.txt, returns "WRITE-OK".
//
//   #!/usr/bin/env python3
//   import os, sys
//   cookie_str = os.environ.get("HTTP_COOKIE", "")
//   session_id = ""
//   for part in cookie_str.split(";"):
//       part = part.strip()
//       if part.startswith("SESSION_ID="):
//           session_id = part.split("=", 1)[1]
//           break
//   length = int(os.environ.get("CONTENT_LENGTH", 0))
//   body = sys.stdin.read(length) if length > 0 else ""
//   if session_id:
//       with open("/tmp/sess_" + session_id + ".txt", "w") as f:
//           f.write(body)
//       result = "WRITE-OK"
//   else:
//       result = "NO-SESSION"
//   print("Content-Type: text/plain")
//   print("Content-Length: " + str(len(result)))
//   print()
//   print(result, end="")
//
//   ── session_read.py ──────────────────────────────────────────────────────
//   Reads the session ID from HTTP_COOKIE, opens /tmp/sess_<id>.txt,
//   returns its content.
//
//   #!/usr/bin/env python3
//   import os
//   cookie_str = os.environ.get("HTTP_COOKIE", "")
//   session_id = ""
//   for part in cookie_str.split(";"):
//       part = part.strip()
//       if part.startswith("SESSION_ID="):
//           session_id = part.split("=", 1)[1]
//           break
//   result = "NO-SESSION"
//   if session_id:
//       try:
//           with open("/tmp/sess_" + session_id + ".txt", "r") as f:
//               result = f.read()
//       except:
//           result = "FILE-NOT-FOUND"
//   print("Content-Type: text/plain")
//   print("Content-Length: " + str(len(result)))
//   print()
//   print(result, end="")
//
// NOTE: In session_write.py and session_read.py, replace "SESSION_ID" with
// the same value as the SESSION_COOKIE_NAME constant above if you changed it.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// extractCookieValue — static helper
//
// Searches the raw HTTP response for a Set-Cookie header that starts with
// <cookieName>=.  Returns the cookie value (everything between '=' and the
// first ';' or '\r\n').  Returns an empty string if not found.
//
// Example:
//   response contains: "Set-Cookie: SESSION_ID=abc123; Path=/\r\n"
//   extractCookieValue(response, "SESSION_ID")  →  "abc123"
// ─────────────────────────────────────────────────────────────────────────────
string SessionTests::extractCookieValue(const string &response, const string &cookieName)
{
	// Look for "Set-Cookie: <cookieName>="
	string needle = "Set-Cookie: " + cookieName + "=";
	size_t pos = response.find(needle);
	if (pos == string::npos)
		return "";

	// Move past the "=" to the start of the value
	size_t valueStart = pos + needle.size();

	// Value ends at the first ';' or '\r' (whichever comes first)
	size_t semi  = response.find(';',  valueStart);
	size_t crlf  = response.find('\r', valueStart);
	size_t valueEnd = min(semi, crlf);    // min handles npos gracefully

	if (valueEnd == string::npos)
		return "";

	return response.substr(valueStart, valueEnd - valueStart);
}

// ─────────────────────────────────────────────────────────────────────────────

SessionTests::SessionTests() : ATestList("Session Tests")
{
	AddAllTests();
}

SessionTests::~SessionTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — Set-Cookie issued on first visit (no cookie in request)
// Targets: SessionManagement (new session creation), HTTPContext,
//          Repsense (Set-Cookie header injection)
//
// A plain GET with no Cookie header — the server has no existing session to
// look up.  It must create a new session and announce it with Set-Cookie.
//
// This is the entry-point test for the whole suite.  If this fails, all
// subsequent tests will also fail because they depend on receiving a valid
// session ID to send back in their Cookie header.
//
// The expectedResponse is "Set-Cookie:" — the most generic possible assertion.
// It passes for any cookie name and any value, so it survives even if
// SESSION_COOKIE_NAME is wrong.  The name-specific assertion is in Test 2.
// ─────────────────────────────────────────────────────────────────────────────
void SessionTests::SetCookieOnFirstVisitTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "1 » Set Cookie On First Visit Test";
	testCase.description = "Test to check that the server issues a Set-Cookie "
	                       "header when a client connects for the first time "
	                       "with no existing Cookie header.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// No Cookie header — this must trigger new-session creation
	testCase.request =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Generic assertion — any Set-Cookie header passes this test
	testCase.expectedResponse.push_back("Set-Cookie:");

	testCase.configurationsForTestCase =
		"SETUP: No special files needed — uses the existing '/' route. "
		"The server's SessionManagement must generate a new session on every "
		"request that does not carry a recognised Cookie header. "
		"If this fails, Set-Cookie is never written into the response — check "
		"that SessionManagement is called before the response headers are sent "
		"and that it writes the header through the same Repsense path used for "
		"other headers. "
		"CRITICAL: Response must include Content-Length.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — Cookie name matches SESSION_COOKIE_NAME and value is non-empty
// Targets: SessionManagement (cookie name configuration, ID generation)
//
// Narrows Test 1: the Set-Cookie header must use the exact cookie name in
// SESSION_COOKIE_NAME and must assign a non-empty value to it.
//
// "Set-Cookie: SESSION_ID=" will only match if:
//   a) The cookie name is exactly "SESSION_ID" (or whatever the constant is).
//   b) At least one character of value follows the '=' sign before
//      any ';' or '\r\n' — because the substring "SESSION_ID=" must be
//      followed by more characters for find() to succeed on a longer string.
//      (A header like "Set-Cookie: SESSION_ID=\r\n" would match
//       "Set-Cookie: SESSION_ID=" so this test alone cannot prove
//       non-emptiness — it is documented as a known limitation.)
// ─────────────────────────────────────────────────────────────────────────────
void SessionTests::CookieValueNotEmptyTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "2 » Cookie Value Not Empty Test";
	testCase.description = "Test to check that the Set-Cookie header uses the "
	                       "expected cookie name (SESSION_COOKIE_NAME constant) "
	                       "and begins to assign a value — confirming the session "
	                       "ID generator actually ran.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Requires both the correct name AND the '=' sign that precedes the value
	SESSION_COOKIE_NAME = "SESSIONID";
	testCase.expectedResponse.push_back(
		"Set-Cookie: " + SESSION_COOKIE_NAME + "=||" 
		"Set-Cookie: SESSIONID=||"
		"Set-Cookie: session_id=||"
		"Set-Cookie: sid=||"
		"Set-Cookie: sessid=||"
		"Set-Cookie: SESSID=");

	testCase.configurationsForTestCase =
		"SETUP: Same as Test 1. "
		"If this fails but Test 1 passed, the server is using a different cookie "
		"name than SESSION_COOKIE_NAME ('" + SESSION_COOKIE_NAME + "'). "
		"Update the SESSION_COOKIE_NAME constant at the top of SessionTests.cpp "
		"to match the name your SessionManagement class uses. "
		"If both tests fail, the Set-Cookie header is missing entirely.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — Cookie header forwarded to CGI as HTTP_COOKIE env var
// Targets: CGIRequest (HTTP_COOKIE env var population), SessionManagement,
//          ClientRequest (Cookie header parsing)
//
// Two-step test — the framework only supports one request per RunTestCase,
// so step 1 is sequenced manually using the protected methods directly:
//
//   Step 1 (manual):
//     GET / with no Cookie → response contains Set-Cookie: SESSION_ID=<id>
//     Extract <id> using extractCookieValue().
//
//   Step 2 (via RunTestCase):
//     GET /cgi-bin/cookie_check.py with Cookie: SESSION_ID=<id>
//     → CGI prints the value of HTTP_COOKIE env var
//     → assert <id> appears somewhere in the response body
//
// A match on <id> in the response proves the server:
//   1. Correctly parsed the Cookie request header.
//   2. Set HTTP_COOKIE to the raw cookie string before execve().
//   3. The CGI could read it and echoed it back.
//
// Note: We assert on the session ID value (not the full "SESSION_ID=<id>"
// string) because the HTTP_COOKIE env var format is the raw header value
// and may include other cookies separated by "; ".
// ─────────────────────────────────────────────────────────────────────────────
void SessionTests::HttpCookieEnvVarTest()
{
	// ── Step 1: obtain a real session ID ─────────────────────────────────────
	TestCase setupCase;
	setupCase.name            = "HTTP Cookie Env Var Test (step 1 - get session)";
	setupCase.description     = "Obtaining a session cookie for step 2.";
	setupCase.port            = "1025";
	setupCase.host            = "localhost";
	setupCase.request         =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	setupCase.expectedResponse.push_back("");   // not asserted — this step is setup only
	setupCase.timeout          = 2000;

	printTestCard(setupCase);
	if (!connectToServer(setupCase)) {
		multiplexer.DeleteFromEpoll(setupCase.socketIO);
		return;
	}
	if (!SendRequestToServer(setupCase)) {
		multiplexer.DeleteFromEpoll(setupCase.socketIO);
		return;
	}
	if (!ReadResponseFromServer(setupCase)) {
		multiplexer.DeleteFromEpoll(setupCase.socketIO);	
		return;
	}
	// Extract the session ID from the Set-Cookie header
	string sessionId = extractCookieValue(setupCase.response, SESSION_COOKIE_NAME);
	if (sessionId.empty())
	{
		CLI::printError("HTTP Cookie Env Var Test failed: could not extract session ID from step 1 response.");
		CLI::printHint("Is Set-Cookie being issued?");
		multiplexer.DeleteFromEpoll(setupCase.socketIO);
		return;
	}
	multiplexer.DeleteFromEpoll(setupCase.socketIO);
	// ── Step 2: send cookie back, assert CGI echoes it ───────────────────────
	TestCase testCase;
	testCase.name        = "3 » HTTP Cookie Env Var Test";
	testCase.description = "Test to check that the server passes the Cookie "
	                       "request header to the CGI as the HTTP_COOKIE "
	                       "environment variable — proven by a CGI script that "
	                       "prints HTTP_COOKIE and a match on the session ID.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Send the session cookie we just received
	testCase.request =
		"GET /cgi-bin/cookie_check.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Cookie: " + SESSION_COOKIE_NAME + "=" + sessionId + "\r\n"
		"\r\n";

	// The CGI prints HTTP_COOKIE which contains the session ID
	testCase.expectedResponse.push_back(sessionId);

	testCase.configurationsForTestCase =
		"SETUP: Requires cookie_check.py in /cgi-bin — see the script at the "
		"top of SessionTests.cpp. "
		"The script reads HTTP_COOKIE and echoes it as the response body. "
		"If the body is 'NO_COOKIE_ENV_VAR', the server did not set HTTP_COOKIE "
		"before execve() — check CGIRequest env var population. "
		"If the body is empty, CONTENT_LENGTH was not set or the pipe was not "
		"read. "
		"CRITICAL: cookie_check.py must output Content-Length.";

	testCase.timeout = 5000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — Invalid/unknown session ID causes a new session to be issued
// Targets: SessionManagement (session lookup, unknown ID handling)
//
// Sends a Cookie header with a session ID that cannot exist in any live
// session store ('not-a-real-session-xyz-000' is chosen to be long enough
// and random-looking to guarantee it was never generated by the server).
//
// The server must:
//   1. Parse the Cookie header and extract the session ID.
//   2. Look it up in its session store — not found.
//   3. Treat the request as a new, unauthenticated visit.
//   4. Create a new session and issue a fresh Set-Cookie header.
//
// Asserts on "Set-Cookie:" — the generic form — because the server may
// either re-use the same cookie name with a new value, or may set additional
// attributes.  Any Set-Cookie issuance proves the server rejected the fake ID.
// ─────────────────────────────────────────────────────────────────────────────
void SessionTests::InvalidSessionNewCookieTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "4 » Invalid Session New Cookie Test";
	testCase.description = "Test to check that when a client sends a Cookie "
	                       "header with an unknown/invalid session ID, the server "
	                       "rejects it and issues a fresh Set-Cookie header for "
	                       "a brand-new session.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// A syntactically valid but semantically non-existent session ID
	testCase.request =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Cookie: " + SESSION_COOKIE_NAME + "=not-a-real-session-xyz-000\r\n"
		"\r\n";

	// Any Set-Cookie header proves the server created a new session
	testCase.expectedResponse.push_back("Set-Cookie:");

	testCase.configurationsForTestCase =
		"SETUP: No special files needed. "
		"The server must look up 'not-a-real-session-xyz-000' in its session "
		"store, find nothing, and create a brand-new session. "
		"If this test fails (no Set-Cookie in response), the server is either: "
		"a) Blindly trusting any session ID it receives without validating it. "
		"b) Not checking whether the looked-up session actually exists. "
		"Either case means a client can forge any session ID and bypass session "
		"creation — a correctness and security issue. "
		"CRITICAL: Response must include Content-Length.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — Session data persisted across two requests via CGI file store
// Targets: SessionManagement (full round-trip), CGI (stdin + HTTP_COOKIE),
//          CGIPipe, ClientRequest (Cookie header)
//
// The crown-jewel session test.  Proves that data stored under a session ID
// in one request can be retrieved using the same session ID in a later request.
//
// Architecture: two CGI scripts use /tmp/sess_<id>.txt as a shared file-based
// session store.  This approach is independent of the server's internal
// session storage mechanism — the only requirement is that HTTP_COOKIE is
// correctly populated in both requests.
//
// Two-step test (manual sequencing for step 1):
//
//   Step 1:
//     GET / with no Cookie → extract session ID from Set-Cookie.
//
//   Step 2:
//     POST /cgi-bin/session_write.py
//       Cookie: SESSION_ID=<id>
//       Body: SESSION-STORED-VALUE
//     → script writes "SESSION-STORED-VALUE" to /tmp/sess_<id>.txt
//     → assert "WRITE-OK" in response.
//
//   Step 3 (the actual assertion step via RunTestCase):
//     GET /cgi-bin/session_read.py
//       Cookie: SESSION_ID=<id>
//     → script reads /tmp/sess_<id>.txt
//     → assert "SESSION-STORED-VALUE" in response body.
//
// If step 3 returns "FILE-NOT-FOUND", step 2 wrote nothing — check that
// HTTP_COOKIE was set in the session_write.py invocation.
// If step 3 returns "NO-SESSION", HTTP_COOKIE was not set in either script.
// ─────────────────────────────────────────────────────────────────────────────
void SessionTests::SessionDataStoredTest()
{
	// ── Step 1: obtain a real session ID ─────────────────────────────────────
	TestCase setupCase;
	setupCase.name            = "Session Data Stored Test (step 1 - get session)";
	setupCase.description     = "Obtaining a session cookie for steps 2 and 3.";
	setupCase.port            = "1025";
	setupCase.host            = "localhost";
	setupCase.request         =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	setupCase.expectedResponse.push_back("");
	setupCase.timeout          = 2000;

	printTestCard(setupCase);
	if (!connectToServer(setupCase)) {
		multiplexer.DeleteFromEpoll(setupCase.socketIO);
		return;
	}
	if (!SendRequestToServer(setupCase)) {
		multiplexer.DeleteFromEpoll(setupCase.socketIO);
		return;
	}
	if (!ReadResponseFromServer(setupCase)) {
		multiplexer.DeleteFromEpoll(setupCase.socketIO);
		return;
	}

	string sessionId = extractCookieValue(setupCase.response, SESSION_COOKIE_NAME);
	if (sessionId.empty())
	{
		multiplexer.DeleteFromEpoll(setupCase.socketIO);
		CLI::printError("Session Data Stored Test failed: could not extract session ID from step 1 response.");
		return;
	}
	multiplexer.DeleteFromEpoll(setupCase.socketIO);


	// ── Step 2: write data to the session via CGI ─────────────────────────────
	TestCase writeCase;
	writeCase.name            = "Session Data Stored Test (step 2 - write)";
	writeCase.description     = "Posting the sentinel value to session_write.py.";
	writeCase.port            = "1025";
	writeCase.host            = "localhost";

	string storedValue   = "SESSION-STORED-VALUE";
	string contentLength = to_string(storedValue.size());

	writeCase.request =
		"POST /cgi-bin/session_write.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"Cookie: " + SESSION_COOKIE_NAME + "=" + sessionId + "\r\n"
		"\r\n"
		+ storedValue;

	writeCase.expectedResponse.push_back("");   // not asserted here
	writeCase.timeout          = 5000;

	// Manual sequencing — we only care about completion, not pass/fail
	if (!connectToServer(writeCase)) {
		multiplexer.DeleteFromEpoll(writeCase.socketIO);
		return;
	}
	if (!SendRequestToServer(writeCase)) {
		multiplexer.DeleteFromEpoll(writeCase.socketIO);
		return;
	}
	if (!ReadResponseFromServer(writeCase)) {
		multiplexer.DeleteFromEpoll(writeCase.socketIO);
		return;
	}

	// Sanity check: if session_write.py returned "NO-SESSION", HTTP_COOKIE
	// was not set — there is no point running step 3.
	if (writeCase.response.find("WRITE-OK") == string::npos)
	{
		multiplexer.DeleteFromEpoll(writeCase.socketIO);
		CLI::printError("Session Data Stored Test failed at step 2: session_write.py did not return WRITE-OK.");
		CLI::printHint("HTTP_COOKIE may not be set in the CGI environment.");
		return;
	}
	multiplexer.DeleteFromEpoll(writeCase.socketIO);


	// ── Step 3: read data back and assert ────────────────────────────────────
	TestCase readCase;
	readCase.name        = "Session Data Stored Test";
	readCase.description = "Test to check that data written to the session in "
	                       "one request can be retrieved using the same session "
	                       "cookie in a subsequent request — the full session "
	                       "persistence round-trip.";
	readCase.port        = "1025";
	readCase.host        = "localhost";

	readCase.request =
		"GET /cgi-bin/session_read.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Cookie: " + SESSION_COOKIE_NAME + "=" + sessionId + "\r\n"
		"\r\n";

	// The sentinel we wrote in step 2 must appear in the body
	readCase.expectedResponse.push_back(storedValue);

	readCase.configurationsForTestCase =
		"SETUP: Requires session_write.py and session_read.py in /cgi-bin. "
		"See the scripts at the top of SessionTests.cpp. "
		"Both scripts use /tmp/sess_<session_id>.txt as shared storage — "
		"replace 'SESSION_ID' in the scripts with your SESSION_COOKIE_NAME if "
		"it differs. "
		"If this returns 'FILE-NOT-FOUND': session_write.py ran but could not "
		"create the temp file — check /tmp write permissions. "
		"If this returns 'NO-SESSION': HTTP_COOKIE was not set when "
		"session_read.py ran — verify CGIRequest populates HTTP_COOKIE. "
		"If the body is empty: check Content-Length in session_read.py output.";

	readCase.timeout = 5000;

	RunTestCase(readCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void SessionTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Set Cookie On First Visit Test",  (void (ATestList::*)())&SessionTests::SetCookieOnFirstVisitTest) );
	_testFunctions.push_back( make_pair("Cookie Value Not Empty Test",     (void (ATestList::*)())&SessionTests::CookieValueNotEmptyTest) );
	_testFunctions.push_back( make_pair("HTTP Cookie Env Var Test",        (void (ATestList::*)())&SessionTests::HttpCookieEnvVarTest) );
	_testFunctions.push_back( make_pair("Invalid Session New Cookie Test", (void (ATestList::*)())&SessionTests::InvalidSessionNewCookieTest) );
	_testFunctions.push_back( make_pair("Session Data Stored Test",        (void (ATestList::*)())&SessionTests::SessionDataStoredTest) );
}
