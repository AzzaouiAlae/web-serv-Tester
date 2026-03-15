#include "MethodRestrictionTests.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// ROUTE SETUP ASSUMPTION
// The server config must have two dedicated routes for these tests:
//
//   location /get-only {
//       allowed_methods GET;
//       root   <your-web-root>;
//       index  index.htm;
//   }
//
//   location /post-only {
//       allowed_methods POST;
//       root   <your-upload-dir>;
//   }
//
// Every test in this class is isolated to one of those two routes so the
// results are unambiguous — no other config option can interfere.
// ─────────────────────────────────────────────────────────────────────────────

MethodRestrictionTests::MethodRestrictionTests() : ATestList("Method Restriction Tests")
{
	AddAllTests();
}

MethodRestrictionTests::~MethodRestrictionTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — POST to a GET-only route  →  405 Method Not Allowed
// Targets: Routing (allowed_methods guard), AMethod dispatch, HTTPContext
//
// The route /get-only only permits GET. Sending POST must be caught BEFORE
// the POST handler is invoked — the check lives in Routing, not in the
// HTTP_Methods layer. The 405 body + Content-Length must be present so that
// ReadResponseFromServer can terminate without timing out.
// ─────────────────────────────────────────────────────────────────────────────
void MethodRestrictionTests::PostToGetOnlyRouteTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Post To Get Only Route Test";
	testCase.description = "Test to check that the server returns 405 Method Not "
	                       "Allowed when POST is sent to a route whose config "
	                       "only permits GET.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	string body          = "should be rejected";
	string contentLength = to_string(body.size());

	testCase.request =
		"POST /get-only HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse.push_back("HTTP/1.1 405 Method Not Allowed");

	testCase.configurationsForTestCase =
		"SETUP: Add a location block for '/get-only' with 'allowed_methods GET'. "
		"The server must reject POST before touching the request body. "
		"The 405 response MUST include a body and 'Content-Length' header — "
		"without it this test will hang waiting for the response to complete.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — DELETE to a GET-only route  →  405 Method Not Allowed
// Targets: Routing (allowed_methods guard), AMethod dispatch
//
// Same route as Test 1 — DELETE is also not in the allowed list.
// Confirms the guard covers all disallowed methods, not just POST.
// ─────────────────────────────────────────────────────────────────────────────
void MethodRestrictionTests::DeleteToGetOnlyRouteTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Delete To Get Only Route Test";
	testCase.description = "Test to check that the server returns 405 Method Not "
	                       "Allowed when DELETE is sent to a route whose config "
	                       "only permits GET.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// DELETE has no body — correct per RFC 7231
	testCase.request =
		"DELETE /get-only HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 405 Method Not Allowed");

	testCase.configurationsForTestCase =
		"SETUP: Same '/get-only' location with 'allowed_methods GET'. "
		"No body in a DELETE is correct. "
		"The 405 response MUST include 'Content-Length' for the test reader to exit.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — GET to a POST-only route  →  405 Method Not Allowed
// Targets: Routing (allowed_methods guard), AMethod dispatch
//
// Flips the scenario: now GET is the forbidden method. This confirms the
// restriction is per-route and not hardcoded to "always allow GET".
// ─────────────────────────────────────────────────────────────────────────────
void MethodRestrictionTests::GetToPostOnlyRouteTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Get To Post Only Route Test";
	testCase.description = "Test to check that the server returns 405 Method Not "
	                       "Allowed when GET is sent to a route whose config "
	                       "only permits POST.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /post-only HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 405 Method Not Allowed");

	testCase.configurationsForTestCase =
		"SETUP: Add a location block for '/post-only' with 'allowed_methods POST'. "
		"This test proves the allowed_methods check is per-route, not a global "
		"'always allow GET' shortcut. The 405 response must include 'Content-Length'.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — DELETE to a POST-only route  →  405 Method Not Allowed
// Targets: Routing (allowed_methods guard), AMethod dispatch
//
// Completes the matrix: POST-only route rejecting DELETE. Ensures neither
// GET nor DELETE can slip through when only POST is permitted.
// ─────────────────────────────────────────────────────────────────────────────
void MethodRestrictionTests::DeleteToPostOnlyRouteTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Delete To Post Only Route Test";
	testCase.description = "Test to check that the server returns 405 Method Not "
	                       "Allowed when DELETE is sent to a route whose config "
	                       "only permits POST.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"DELETE /post-only HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 405 Method Not Allowed");

	testCase.configurationsForTestCase =
		"SETUP: Same '/post-only' location with 'allowed_methods POST'. "
		"The 405 response must include 'Content-Length'.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — Allow header present in 405 response
// Targets: HTTPContext / Repsense (response builder), AMethod dispatch
//
// RFC 7231 §6.5.5 mandates that a 405 response MUST include an Allow header
// listing the methods the resource actually supports. This test does NOT check
// the status code (that is already covered above) — it uses the substring
// match to confirm the Allow: token exists anywhere in the raw response.
//
// expectedResponse = "Allow:" is intentionally generic: it will match
// "Allow: GET" or "Allow: POST" or any other value, which is correct because
// the test's only job is proving the header is present at all.
// ─────────────────────────────────────────────────────────────────────────────
void MethodRestrictionTests::AllowHeaderPresentOn405Test()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Allow Header Present On 405 Test";
	testCase.description = "Test to check that a 405 response contains the "
	                       "mandatory 'Allow:' header listing the methods the "
	                       "route actually supports (RFC 7231 §6.5.5).";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Reuse the GET-only route — POST triggers 405
	string body          = "trigger 405";
	string contentLength = to_string(body.size());

	testCase.request =
		"POST /get-only HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	// Substring match — passes if the header name exists anywhere in the response
	testCase.expectedResponse.push_back("Allow:");

	testCase.configurationsForTestCase =
		"SETUP: Same '/get-only' location. When building the 405 response your "
		"Repsense/response builder must inject an 'Allow:' header whose value "
		"is the comma-separated list of methods from the route config "
		"(e.g. 'Allow: GET'). Without this header the response is non-compliant. "
		"The 405 body must still have 'Content-Length' so the test reader exits.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void MethodRestrictionTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Post To Get Only Route Test",    (void (ATestList::*)())&MethodRestrictionTests::PostToGetOnlyRouteTest) );
	_testFunctions.push_back( make_pair("Delete To Get Only Route Test",  (void (ATestList::*)())&MethodRestrictionTests::DeleteToGetOnlyRouteTest) );
	_testFunctions.push_back( make_pair("Get To Post Only Route Test",    (void (ATestList::*)())&MethodRestrictionTests::GetToPostOnlyRouteTest) );
	_testFunctions.push_back( make_pair("Delete To Post Only Route Test", (void (ATestList::*)())&MethodRestrictionTests::DeleteToPostOnlyRouteTest) );
	_testFunctions.push_back( make_pair("Allow Header Present On 405 Test", (void (ATestList::*)())&MethodRestrictionTests::AllowHeaderPresentOn405Test) );
}
