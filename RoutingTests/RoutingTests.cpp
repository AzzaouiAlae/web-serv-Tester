#include "RoutingTests.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// SETUP OVERVIEW — file-system and config required before running this suite
//
//  Config blocks:
//
//   # Custom root mapping — URL prefix /mapped → /tmp/mapped-root on disk
//   location /mapped {
//       root  /tmp/mapped-root;
//       index index.htm;
//   }
//
//   # Deep nested path — lives under the default web root
//   location /static {
//       root  <your-web-root>;
//       index index.htm;
//   }
//
//   # Directory used for trailing-slash and no-trailing-slash tests
//   location /dir-with-index {
//       root  <your-web-root>;
//       index index.htm;
//   }
//
//  File-system:
//   # Test 1
//   mkdir -p /tmp/mapped-root
//   echo '<html><body>ROOT-MAPPING-SENTINEL</body></html>' 
//        > /tmp/mapped-root/index.htm
//
//   # Test 2
//   mkdir -p <web-root>/static/a/b
//   echo '<html><body>NESTED-PATH-SENTINEL</body></html>' 
//        > <web-root>/static/a/b/nested.html
//
//   # Tests 3 & 4
//   mkdir -p <web-root>/dir-with-index
//   echo '<html><body>TRAILING-SLASH-SENTINEL</body></html>' 
//        > <web-root>/dir-with-index/index.htm
//
//   # Test 5
//   echo '<html><body>QUERY-STRING-SENTINEL</body></html>' 
//        > <web-root>/index.htm          # re-uses the main index
//
//   # Test 6
//   echo '<html><body>PERCENT-ENCODE-SENTINEL</body></html>' 
//        > "<web-root>/my page.html"     # filename contains a literal space
//
// CRITICAL — Content-Length:
// ReadResponseFromServer terminates only when it has received
// header_length + content_length bytes.  Every response — including 301
// redirects — MUST include a Content-Length header, or the test will block
// until timeout.
// ─────────────────────────────────────────────────────────────────────────────

RoutingTests::RoutingTests() : ATestList("Routing Tests")
{
	AddAllTests();
}

RoutingTests::~RoutingTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — Custom root mapping
// Targets: Path (root directive resolution), Routing (prefix stripping), Config/AST
//
// The subject example: "if URL /kapouet is rooted to /tmp/www, URL
// /kapouet/pouic/toto/pouet will search for /tmp/www/pouic/toto/pouet".
//
// This test uses location /mapped rooted to /tmp/mapped-root.
// A GET /mapped/ must resolve to /tmp/mapped-root/index.htm and serve it.
// The sentinel string 'ROOT-MAPPING-SENTINEL' exists only in that file, so a
// match proves the correct filesystem root was used — not the default web root.
//
// Failure modes:
//  • 404 — the URL prefix was not stripped before appending to the root, so
//    the server looks for /tmp/mapped-root/mapped/index.htm (double-prefix).
//  • Serves wrong content — the default web root was used instead of /tmp/mapped-root.
// ─────────────────────────────────────────────────────────────────────────────
void RoutingTests::RootMappingTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Root Mapping Test";
	testCase.description = "Test to check that the server correctly applies a "
	                       "custom 'root' directive — stripping the location "
	                       "prefix and prepending the configured root path before "
	                       "looking up the file on disk.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /mapped/ HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Sentinel only exists in /tmp/mapped-root/index.htm
	testCase.expectedResponse.push_back("ROOT-MAPPING-SENTINEL");

	testCase.configurationsForTestCase =
		"SETUP: 'location /mapped { root /tmp/mapped-root; index index.htm; }'. "
		"Create: mkdir -p /tmp/mapped-root && "
		"echo '<html><body>ROOT-MAPPING-SENTINEL</body></html>' > /tmp/mapped-root/index.htm. "
		"If this returns 404, the Path class is not stripping the '/mapped' prefix "
		"before building the filesystem path — it is looking for "
		"'/tmp/mapped-root/mapped/index.htm' instead of '/tmp/mapped-root/index.htm'. "
		"If it returns the wrong content, the default web root is being used.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — Nested path resolution
// Targets: Path (multi-segment path append), Routing
//
// Verifies that a URL with several path segments beyond the location prefix
// is correctly appended to the root — none of the intermediate segments must
// be dropped, duplicated, or corrupted.
//
// URL:   GET /static/a/b/nested.html
// Root:  <web-root>/static  (or <web-root> with location /static)
// File:  <web-root>/static/a/b/nested.html
//
// The sentinel 'NESTED-PATH-SENTINEL' is inside that specific file. If any
// part of the path translation is wrong (e.g. only one level deep is appended)
// the server will return 404 or serve a different file.
// ─────────────────────────────────────────────────────────────────────────────
void RoutingTests::NestedPathResolutionTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Nested Path Resolution Test";
	testCase.description = "Test to check that the server correctly resolves a "
	                       "deeply nested URL path (multiple subdirectory levels) "
	                       "by appending all path segments to the configured root.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /static/a/b/nested.html HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Sentinel only exists inside <web-root>/static/a/b/nested.html
	testCase.expectedResponse.push_back("NESTED-PATH-SENTINEL");

	testCase.configurationsForTestCase =
		"SETUP: Create the directory chain and file: "
		"mkdir -p <web-root>/static/a/b && "
		"echo '<html><body>NESTED-PATH-SENTINEL</body></html>' "
		"> <web-root>/static/a/b/nested.html. "
		"If this returns 404, the Path class is not appending the full URL suffix "
		"— check for off-by-one errors in the substring that follows the location "
		"prefix, especially when the location ends without a trailing slash.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — Trailing slash + index file → serves index file (body sentinel check)
// Targets: Path (directory detection, index file lookup), StaticFile, Routing
//
// When a URL ends with '/' and the location has an index directive, the server
// must serve the named index file, not a listing and not a 403.
//
// This goes beyond the existing GetDirectoryWithTrailingSlashTest in
// HappyPathTests which only checks for 200 OK. Here we check the actual body
// content with a sentinel to confirm the correct file was served.
// ─────────────────────────────────────────────────────────────────────────────
void RoutingTests::TrailingSlashWithIndexTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Trailing Slash With Index Test";
	testCase.description = "Test to check that requesting a directory URL with a "
	                       "trailing slash causes the server to serve the configured "
	                       "index file — verified by a sentinel string in the body, "
	                       "not just a 200 status.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /dir-with-index/ HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Sentinel lives only in <web-root>/dir-with-index/index.htm
	testCase.expectedResponse.push_back("TRAILING-SLASH-SENTINEL");

	testCase.configurationsForTestCase =
		"SETUP: 'location /dir-with-index { root <web-root>; index index.htm; }'. "
		"Create: mkdir -p <web-root>/dir-with-index && "
		"echo '<html><body>TRAILING-SLASH-SENTINEL</body></html>' "
		"> <web-root>/dir-with-index/index.htm. "
		"If status is 200 but the sentinel is missing, the server returned a "
		"directory listing or an empty body instead of the index file. "
		"Check the index-file lookup order in the Path class.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — No trailing slash on a directory → 301 redirect with slash appended
// Targets: Path (directory detection without trailing slash), Routing,
//          Repsense (Location header construction)
//
// When the URL maps to a directory but has NO trailing slash, the server must
// issue a 301 redirect whose Location adds the trailing slash.
// This is the standard nginx behaviour and the correct HTTP semantic.
//
// Two separate assertions are needed (status code and Location header) but
// the framework only supports one expectedResponse per TestCase. We assert on
// "Location: /dir-with-index/" because:
//  a) It only appears in a redirect response, so a 200 would fail the test.
//  b) It is more specific than "HTTP/1.1 301" — it also validates the Location value.
//
// CRITICAL: The 301 response must include Content-Length (even Content-Length: 0)
// or ReadResponseFromServer will block forever.
// ─────────────────────────────────────────────────────────────────────────────
void RoutingTests::NoTrailingSlashRedirectTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "No Trailing Slash Redirect Test";
	testCase.description = "Test to check that requesting a directory URL without "
	                       "a trailing slash causes the server to return a 301 "
	                       "redirect to the same URL with a trailing slash appended.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// No trailing slash — must trigger the redirect
	testCase.request =
		"GET /dir-with-index HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Asserting on Location is stricter than asserting on the status line
	// — it validates both that a redirect was issued AND the target is correct
	testCase.expectedResponse.push_back("Location: /dir-with-index/");

	testCase.configurationsForTestCase =
		"SETUP: Same 'location /dir-with-index' from Test 3. "
		"The server must detect that '/dir-with-index' (no trailing slash) maps "
		"to a directory on disk and issue 301 with 'Location: /dir-with-index/'. "
		"CRITICAL: The 301 response MUST include 'Content-Length' (at minimum "
		"'Content-Length: 0') or the test reader will block until timeout. "
		"If the server returns 200 instead of 301, the directory-detection check "
		"is running after the index-file lookup rather than before it.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — Query string stripped before filesystem lookup
// Targets: Path (URL parsing — split on '?'), ClientRequest, Routing
//
// A URL such as /index.htm?foo=bar&baz=qux must be split on '?' and only the
// path component (/index.htm) must be used for the filesystem lookup.
// The query string must be discarded before Path builds the on-disk path.
//
// If the server passes the full string "/index.htm?foo=bar&baz=qux" to the
// filesystem it will get ENOENT and return 404, causing this test to fail.
//
// We assert on the sentinel 'QUERY-STRING-SENTINEL' rather than just 200 OK
// so that a cached or wrong file being served is also caught.
// ─────────────────────────────────────────────────────────────────────────────
void RoutingTests::QueryStringIgnoredTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Query String Ignored Test";
	testCase.description = "Test to check that the server strips the query string "
	                       "from the URL before resolving the filesystem path — "
	                       "a request for '/index.htm?foo=bar' must serve the same "
	                       "file as a plain '/index.htm' request.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Query string appended — must be stripped before the path lookup
	testCase.request =
		"GET /QueryString/index.htm?foo=bar&baz=qux HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Sentinel lives in <web-root>/index.htm
	testCase.expectedResponse.push_back("QUERY-STRING-SENTINEL");

	testCase.configurationsForTestCase =
		"SETUP: The main index.htm must contain the sentinel: "
		"echo '<html><body>QUERY-STRING-SENTINEL</body></html>' > <web-root>/index.htm. "
		"If this returns 404, the Path class is treating the full string "
		"'/index.htm?foo=bar&baz=qux' as the filename — split on '?' first and "
		"use only the left-hand side for the filesystem lookup. "
		"The query string must be preserved in the request context for CGI "
		"(as QUERY_STRING env var) but must never reach the filesystem path builder.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6 — Percent-encoded path decoded before filesystem lookup
// Targets: Path (URL percent-decoding), ClientRequest (raw URL storage),
//          Routing
//
// A URL of /my%20page.html must be decoded to /my page.html (with a literal
// space) before the filesystem stat/open call.  The file on disk has a space
// in its name.
//
// Common failure modes:
//  • 404 — the server passes the literal string "my%20page.html" to open(),
//    which finds no such file because the real filename contains a space.
//  • Double-decode — decoding is applied twice, corrupting the path.
//  • Security bypass — a path like /..%2F..%2Fetc%2Fpasswd is decoded to
//    ../../etc/passwd; this test does not check that case but the decoder
//    that passes this test must also be followed by a path-traversal guard.
// ─────────────────────────────────────────────────────────────────────────────
void RoutingTests::PercentEncodedPathTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Percent Encoded Path Test";
	testCase.description = "Test to check that the server correctly percent-decodes "
	                       "the request URL before resolving the filesystem path — "
	                       "a request for '/my%%20page.html' must serve the file "
	                       "whose name contains a literal space character.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// %20 is the percent-encoded representation of a space character
	testCase.request =
		"GET /my%20page.html HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Sentinel lives in <web-root>/my page.html (space in filename)
	testCase.expectedResponse.push_back("PERCENT-ENCODE-SENTINEL");

	testCase.configurationsForTestCase =
		"SETUP: Create a file with a literal space in the name: "
		"echo '<html><body>PERCENT-ENCODE-SENTINEL</body></html>' "
		"> \"<web-root>/my page.html\"  (note the space between 'my' and 'page'). "
		"The request URL uses '%20' which must be decoded to ' ' before the "
		"filesystem open() call. "
		"If this returns 404, the Path class is not performing percent-decoding — "
		"it is passing the raw '%20' bytes directly to the OS. "
		"After decoding, apply a path-traversal guard ('../' removal) before "
		"opening the file, since decoded paths can contain sequences like '/../'.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void RoutingTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Root Mapping Test",              (void (ATestList::*)())&RoutingTests::RootMappingTest) );
	_testFunctions.push_back( make_pair("Nested Path Resolution Test",    (void (ATestList::*)())&RoutingTests::NestedPathResolutionTest) );
	_testFunctions.push_back( make_pair("Trailing Slash With Index Test", (void (ATestList::*)())&RoutingTests::TrailingSlashWithIndexTest) );
	_testFunctions.push_back( make_pair("No Trailing Slash Redirect Test",(void (ATestList::*)())&RoutingTests::NoTrailingSlashRedirectTest) );
	_testFunctions.push_back( make_pair("Query String Ignored Test",      (void (ATestList::*)())&RoutingTests::QueryStringIgnoredTest) );
	_testFunctions.push_back( make_pair("Percent Encoded Path Test",      (void (ATestList::*)())&RoutingTests::PercentEncodedPathTest) );
}
