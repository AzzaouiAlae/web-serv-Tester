#include "ClientBodySizeTests.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// ASSUMPTION: The server config has:
//     client_max_body_size 1024
// on the location block that handles POST /upload.
// All four tests are designed around that boundary value.
// To use a different limit, change the BODY_LIMIT constant below and update
// your server config to match.
// ─────────────────────────────────────────────────────────────────────────────
static const size_t BODY_LIMIT = 1024;

ClientBodySizeTests::ClientBodySizeTests() : ATestList("Client Body Size Tests")
{
	AddAllTests();
}

ClientBodySizeTests::~ClientBodySizeTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — Well under the limit  →  200 OK
// Targets: ClientRequest (body accumulation), POST handler, Config limit check
// A tiny 13-byte body must sail through the size check without triggering 413.
// ─────────────────────────────────────────────────────────────────────────────
void ClientBodySizeTests::PostWellUnderLimitTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Post Well Under Limit Test";
	testCase.description = "Test to check that a POST body significantly smaller "
	                       "than client_max_body_size is accepted and returns 201 Created.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	string body          = "Hello, World!";   // 13 bytes — well under 1024
	string contentLength = to_string(body.size());

	testCase.request =
		"POST /upload/" + GetRandem() + ".txt HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created");

	testCase.configurationsForTestCase =
		"NOTE: Server config must have 'client_max_body_size 1024' on the "
		"'/upload' location. A 13-byte body must be accepted without hitting "
		"the size guard. If this fails with 413 your limit check is too eager.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — Body == exact limit  →  200 OK
// Targets: ClientRequest (boundary condition in size check)
// This is the critical boundary-value test. A body of exactly BODY_LIMIT bytes
// must be ACCEPTED — the check must be strictly greater-than, not >=.
// ─────────────────────────────────────────────────────────────────────────────
void ClientBodySizeTests::PostAtExactLimitTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Post At Exact Limit Test";
	testCase.description = "Test to check that a POST body whose size equals "
	                       "client_max_body_size exactly is accepted (boundary "
	                       "value — the check must be body_size > limit, not >=).";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Exactly BODY_LIMIT bytes of 'A'
	string body          = string(BODY_LIMIT, 'A');
	string contentLength = to_string(body.size());

	testCase.request =
		"POST /upload/" + GetRandem() + ".txt HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created");

	testCase.configurationsForTestCase =
		"NOTE: This test sends exactly 1024 bytes (matching 'client_max_body_size 1024'). "
		"The server MUST accept it and return 201 Created. If it returns 413 your "
		"comparison operator is wrong — use strictly-greater-than (body > limit), "
		"not greater-than-or-equal (body >= limit).";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — Body == limit + 1 byte  →  413 Content Too Large
// Targets: ClientRequest (size guard), HTTPContext, DefaultPages
// This is the other half of the boundary pair. One byte past the limit must
// be rejected immediately. The server must not buffer the full body before
// responding — ideally it rejects as soon as Content-Length exceeds the limit.
// ─────────────────────────────────────────────────────────────────────────────
void ClientBodySizeTests::PostOneByteOverLimitTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Post One Byte Over Limit Test";
	testCase.description = "Test to check that a POST body one byte larger than "
	                       "client_max_body_size is rejected with 413 Content Too Large.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// BODY_LIMIT + 1 bytes — must be rejected
	string body          = string(BODY_LIMIT + 1, 'B');
	string contentLength = to_string(body.size());

	testCase.request =
		"POST /upload/" + GetRandem() + ".txt HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse.push_back("HTTP/1.1 413 Payload Too Large");

	testCase.configurationsForTestCase =
		"NOTE: This test sends 1025 bytes (one over 'client_max_body_size 1024'). "
		"The server should detect the oversize condition from the Content-Length "
		"header BEFORE reading the body and respond with 413 immediately. "
		"The 413 response must include a body and Content-Length. "
		"If your server only checks size after buffering the body, this will "
		"still pass — but the early-rejection path is more RFC-correct.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — Body >> limit  →  413 Content Too Large
// Targets: ClientRequest (size guard), server resilience
// Sends a body 10× the limit to verify the server does not buffer the entire
// payload in memory before rejecting and does not crash or stall.
// Also verifies that the server remains responsive AFTER the rejection.
// ─────────────────────────────────────────────────────────────────────────────
void ClientBodySizeTests::PostLargeBodyRejectedTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Post Large Body Rejected Test";
	testCase.description = "Test to check that a POST body far exceeding "
	                       "client_max_body_size is rejected with 413 and that "
	                       "the server remains operational afterwards.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// 10× the limit — stresses the size guard and memory handling
	string body          = string(BODY_LIMIT * 10, 'C');
	string contentLength = to_string(body.size());

	testCase.request =
		"POST /upload/" + GetRandem() + ".txt HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse.push_back("HTTP/1.1 413 Payload Too Large");

	testCase.configurationsForTestCase =
		"NOTE: This test sends 10240 bytes (10x 'client_max_body_size 1024'). "
		"The server must reject based on the Content-Length header without "
		"allocating a 10KB buffer first. After returning 413, the server must "
		"still accept new connections — run a normal GET / immediately after "
		"to confirm resilience. A crash or hang here indicates unbounded body "
		"buffering before the size check.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void ClientBodySizeTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Post Well Under Limit Test",    (void (ATestList::*)())&ClientBodySizeTests::PostWellUnderLimitTest) );
	_testFunctions.push_back( make_pair("Post At Exact Limit Test",      (void (ATestList::*)())&ClientBodySizeTests::PostAtExactLimitTest) );
	_testFunctions.push_back( make_pair("Post One Byte Over Limit Test", (void (ATestList::*)())&ClientBodySizeTests::PostOneByteOverLimitTest) );
	_testFunctions.push_back( make_pair("Post Large Body Rejected Test", (void (ATestList::*)())&ClientBodySizeTests::PostLargeBodyRejectedTest) );
}
