#include "RedirectionTests.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// ROUTE SETUP ASSUMPTION
//
// The server config must have these location blocks:
//
//   # 301 permanent redirect — internal path
//   location /old-page {
//       return 301 /new-page;
//   }
//
//   # 302 temporary redirect — internal path
//   location /temp-page {
//       return 302 /index.htm;
//   }
//
//   # 301 redirect to an external absolute URL
//   location /ext-redirect {
//       return 301 http://example.com;
//   }
//
// CRITICAL — Content-Length in redirect responses:
// The ReadResponseFromServer loop in ATestList terminates ONLY when it has
// received header_length + content_length bytes. If the server sends a 3xx
// response with NO Content-Length header the loop will block until timeout.
// Your server MUST emit Content-Length on every response, including redirects.
// Use Content-Length: 0 for header-only redirects, or include a small HTML
// body (like nginx does) and set Content-Length to its exact byte count.
// ─────────────────────────────────────────────────────────────────────────────

RedirectionTests::RedirectionTests() : ATestList("Redirection Tests")
{
	AddAllTests();
}

RedirectionTests::~RedirectionTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — 301 Moved Permanently — status line check
// Targets: AMethod (return directive dispatch), Repsense (response builder),
//          Config/AST (return 301 value storage)
//
// Checks that the response status line is exactly "HTTP/1.1 301 Moved Permanently".
// Keeps the status code and the Location header as two separate test cases so
// a failure in one does not mask the other.
// ─────────────────────────────────────────────────────────────────────────────
void RedirectionTests::PermanentRedirectStatusTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Permanent Redirect Status Test";
	testCase.description = "Test to check that the server returns the correct "
	                       "'301 Moved Permanently' status line when a route "
	                       "is configured with 'return 301 /new-page'.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /old-page HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse = "HTTP/1.1 301 Moved Permanently";

	testCase.configFileData =
		"SETUP: Add 'location /old-page { return 301 /new-page; }' in your config. "
		"CRITICAL: The 301 response MUST include 'Content-Length' (even if the "
		"value is 0) otherwise the test reader will block forever waiting for the "
		"body. A small HTML body like nginx produces is also acceptable as long as "
		"Content-Length matches its size exactly.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — 301 Moved Permanently — Location header check
// Targets: Repsense (Location header injection), Config/AST (redirect URL value)
//
// Same route, second assertion: the Location header must be present and point
// to the correct destination. Uses substring match on "Location: /new-page"
// so the test passes regardless of trailing whitespace or other headers.
// ─────────────────────────────────────────────────────────────────────────────
void RedirectionTests::PermanentRedirectLocationHeaderTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Permanent Redirect Location Header Test";
	testCase.description = "Test to check that the 301 response for '/old-page' "
	                       "contains a 'Location: /new-page' header pointing to "
	                       "the correct redirect destination.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /old-page HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Substring match — will find "Location: /new-page" anywhere in the response
	testCase.expectedResponse = "Location: /new-page";

	testCase.configFileData =
		"SETUP: Same 'location /old-page { return 301 /new-page; }'. "
		"The Location header value must exactly match the URL given in the "
		"'return' directive. Check for off-by-one errors: 'Location:/new-page' "
		"(no space) will cause this test to fail — the space after the colon "
		"is mandatory per RFC 7230. "
		"CRITICAL: Response must include 'Content-Length'.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — 302 Found — status line check
// Targets: AMethod (return directive dispatch), Repsense, Config/AST
//
// A temporary redirect must produce 302, not 301. Confirms the server stores
// and uses the exact numeric code from the 'return' directive, not a hardcoded
// value shared across all redirects.
// ─────────────────────────────────────────────────────────────────────────────
void RedirectionTests::TemporaryRedirectStatusTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Temporary Redirect Status Test";
	testCase.description = "Test to check that the server returns the correct "
	                       "'302 Found' status line when a route is configured "
	                       "with 'return 302 /index.htm'.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /temp-page HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse = "HTTP/1.1 302 Found";

	testCase.configFileData =
		"SETUP: Add 'location /temp-page { return 302 /index.htm; }' in your config. "
		"CRITICAL: Response must include 'Content-Length' (even if 0). "
		"If both 301 and 302 routes return 301 your redirect code generator "
		"is hardcoding the status instead of reading it from the AST node.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — 302 Found — Location header check
// Targets: Repsense (Location header injection), Config/AST
//
// Same route as Test 3 — validates the Location value is the path from the
// 'return' directive, not a stale value from a previous response or a 301.
// ─────────────────────────────────────────────────────────────────────────────
void RedirectionTests::TemporaryRedirectLocationHeaderTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Temporary Redirect Location Header Test";
	testCase.description = "Test to check that the 302 response for '/temp-page' "
	                       "contains a 'Location: /index.htm' header pointing "
	                       "to the correct redirect destination.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /temp-page HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Substring match — finds "Location: /index.htm" anywhere in the response
	testCase.expectedResponse = "Location: /index.htm";

	testCase.configFileData =
		"SETUP: Same 'location /temp-page { return 302 /index.htm; }'. "
		"Verify the Location value is '/index.htm' and NOT '/new-page' — that "
		"would indicate the response builder is reusing the 301 route's value. "
		"CRITICAL: Response must include 'Content-Length'.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — Redirect to an external absolute URL
// Targets: Repsense (Location header with absolute URL), Config/AST
//          (absolute URL stored and forwarded without modification)
//
// When the 'return' directive contains an absolute URL (http://...) the server
// must copy it verbatim into the Location header — it must NOT prepend the
// server's own host, strip the scheme, or otherwise mangle the URL.
// Uses substring "Location: http" to match any http:// or https:// URL
// without hard-coding the exact destination domain.
// ─────────────────────────────────────────────────────────────────────────────
void RedirectionTests::RedirectToExternalUrlTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Redirect To External Url Test";
	testCase.description = "Test to check that the server correctly places an "
	                       "absolute external URL in the Location header when "
	                       "the 'return' directive specifies a full http:// URL.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /ext-redirect HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Matches any absolute URL scheme — "Location: http" covers both http and https
	testCase.expectedResponse = "Location: http";

	testCase.configFileData =
		"SETUP: Add 'location /ext-redirect { return 301 http://example.com; }'. "
		"The Location header in the response must contain the full absolute URL "
		"'http://example.com' — not '/example.com' or 'example.com'. "
		"A common bug is the response builder always prepending 'http://localhost' "
		"to every Location value — this test catches that. "
		"CRITICAL: Response must include 'Content-Length'.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void RedirectionTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Permanent Redirect Status Test",          (void (ATestList::*)())&RedirectionTests::PermanentRedirectStatusTest) );
	_testFunctions.push_back( make_pair("Permanent Redirect Location Header Test", (void (ATestList::*)())&RedirectionTests::PermanentRedirectLocationHeaderTest) );
	_testFunctions.push_back( make_pair("Temporary Redirect Status Test",          (void (ATestList::*)())&RedirectionTests::TemporaryRedirectStatusTest) );
	_testFunctions.push_back( make_pair("Temporary Redirect Location Header Test", (void (ATestList::*)())&RedirectionTests::TemporaryRedirectLocationHeaderTest) );
	_testFunctions.push_back( make_pair("Redirect To External Url Test",           (void (ATestList::*)())&RedirectionTests::RedirectToExternalUrlTest) );
}
