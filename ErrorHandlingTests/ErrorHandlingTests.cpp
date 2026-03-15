#include "ErrorHandlingTests.hpp"

ErrorHandlingTests::ErrorHandlingTests() : ATestList("Error Handling Tests")
{
	AddAllTests();
}

ErrorHandlingTests::~ErrorHandlingTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — 404 Not Found
// Targets: StaticFile, DefaultPages, Routing miss
// The server must serve its configured custom 404 page (or the built-in
// default) and include a valid Content-Length so ReadResponseFromServer
// can terminate correctly.
// ─────────────────────────────────────────────────────────────────────────────
void ErrorHandlingTests::GetNonExistentFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Get Non Existent File Test";
	testCase.description = "Test to check if the server returns 404 Not Found "
	                       "when a requested file does not exist on the server.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Requesting a path that must not exist in the web root
	testCase.request =
		"GET /this_file_does_not_exist_at_all.html HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 404 Not Found");

	testCase.configurationsForTestCase =
		"NOTE: Ensure your server has a default error page configured for 404 "
		"(e.g. 'error_page 404 /404.html'). The response MUST include a "
		"Content-Length header so the test reader can detect the end of the "
		"response. If no custom page exists the server must still generate a "
		"minimal 404 body with Content-Length.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — 403 Forbidden
// Targets: StaticFile (permission check), DefaultPages
// The file must physically exist on disk but have its read permissions
// removed (chmod 000) so the server cannot open it.
// ─────────────────────────────────────────────────────────────────────────────
void ErrorHandlingTests::GetForbiddenFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Get Forbidden File Test";
	testCase.description = "Test to check if the server returns 403 Forbidden "
	                       "when the requested file exists but is not readable "
	                       "due to file system permissions.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// File must exist in the web root with permissions set to 000
	testCase.request =
		"GET /forbidden.html HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 403 Forbidden");

	testCase.configurationsForTestCase =
		"NOTE: Before running this test create a file named 'forbidden.html' "
		"inside your web root and run: chmod 000 forbidden.html. "
		"The server must detect the permission error when opening the file and "
		"return 403 with a body and Content-Length. Do NOT delete the file — "
		"a missing file produces 404, not 403.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — 400 Bad Request (malformed request line)
// Targets: ClientRequest parser, HTTPContext
// Sending a request line that has no HTTP version token at all.
// The server must detect the parse failure and close the connection with 400.
// ─────────────────────────────────────────────────────────────────────────────
void ErrorHandlingTests::MalformedRequestLineTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Malformed Request Line Test";
	testCase.description = "Test to check if the server returns 400 Bad Request "
	                       "when the incoming request line is syntactically invalid "
	                       "(missing HTTP version token).";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// No HTTP version — request line is "GET /\r\n" — not valid HTTP/1.1
	testCase.request =
		"GET /\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 400 Bad Request");

	testCase.configurationsForTestCase =
		"NOTE: The server's request parser must validate that the request line "
		"has exactly three tokens (METHOD SP Request-URI SP HTTP-Version CRLF). "
		"On parse failure it must respond with 400 and a body+Content-Length, "
		"then close the connection. The response must NOT be an empty TCP close.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — 501 Not Implemented (unrecognised / unhandled HTTP method)
// Targets: AMethod dispatch, HTTPContext
// The subject only requires GET, POST, DELETE. Any other method (PATCH here)
// must return 501 per the same convention already used in HeadRequestTest.
// ─────────────────────────────────────────────────────────────────────────────
void ErrorHandlingTests::UnsupportedMethodTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Unsupported Method Test";
	testCase.description = "Test to check if the server returns 501 Not Implemented "
	                       "when it receives an HTTP method it does not support "
	                       "(e.g. PATCH).";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// PATCH is a valid HTTP/1.1 token but this server does not implement it
	testCase.request =
		"PATCH / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 0\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 501 Not Implemented");

	testCase.configurationsForTestCase =
		"NOTE: Any method other than GET, POST and DELETE must result in 501. "
		"Ensure the AMethod dispatch layer does not silently fall through to a "
		"GET handler. The response must include Content-Length.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — 505 HTTP Version Not Supported
// Targets: ClientRequest parser, HTTPContext
// Sending HTTP/2.0 in the request line. The server only speaks HTTP/1.1.
// ─────────────────────────────────────────────────────────────────────────────
void ErrorHandlingTests::HttpVersionMismatchTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Http Version Mismatch Test";
	testCase.description = "Test to check if the server returns 505 HTTP Version "
	                       "Not Supported when the client announces HTTP/2.0 in "
	                       "the request line.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// HTTP/2.0 is syntactically valid but not supported by this server
	testCase.request =
		"GET / HTTP/2.0\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 505 HTTP Version Not Supported");

	testCase.configurationsForTestCase =
		"NOTE: After parsing the request line the server must compare the "
		"version token against 'HTTP/1.1' (and optionally 'HTTP/1.0'). "
		"Any other version string must produce 505 with a body+Content-Length. "
		"The response itself must still be formatted as HTTP/1.1.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6 — 400 Bad Request (missing Host header)
// Targets: ClientRequest header validation, HTTPContext
// RFC 7230 §5.4 — A server MUST respond with 400 if an HTTP/1.1 request
// does not include a Host header field.
// ─────────────────────────────────────────────────────────────────────────────
void ErrorHandlingTests::MissingHostHeaderTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Missing Host Header Test";
	testCase.description = "Test to check if the server returns 400 Bad Request "
	                       "when an HTTP/1.1 request arrives with no Host header, "
	                       "which is mandatory per RFC 7230.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Intentionally omitting the Host header — only the blank line terminator
	testCase.request =
		"GET / HTTP/1.1\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 400 Bad Request");

	testCase.configurationsForTestCase =
		"NOTE: After headers are parsed, validate that the 'Host' header is "
		"present for any HTTP/1.1 request. Its absence must trigger a 400 "
		"response with a body and Content-Length before any routing or file "
		"access is attempted.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void ErrorHandlingTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Get Non Existent File Test",  (void (ATestList::*)())&ErrorHandlingTests::GetNonExistentFileTest) );
	_testFunctions.push_back( make_pair("Get Forbidden File Test",     (void (ATestList::*)())&ErrorHandlingTests::GetForbiddenFileTest) );
	_testFunctions.push_back( make_pair("Malformed Request Line Test", (void (ATestList::*)())&ErrorHandlingTests::MalformedRequestLineTest) );
	_testFunctions.push_back( make_pair("Unsupported Method Test",     (void (ATestList::*)())&ErrorHandlingTests::UnsupportedMethodTest) );
	_testFunctions.push_back( make_pair("Http Version Mismatch Test",  (void (ATestList::*)())&ErrorHandlingTests::HttpVersionMismatchTest) );
	_testFunctions.push_back( make_pair("Missing Host Header Test",    (void (ATestList::*)())&ErrorHandlingTests::MissingHostHeaderTest) );
}
