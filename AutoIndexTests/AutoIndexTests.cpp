#include "AutoIndexTests.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// DIRECTORY + CONFIG SETUP — required before running this suite
//
// Three dedicated locations must exist in the server config:
//
//   # 1. autoindex ON, no default index file
//   location /autoindex-dir {
//       autoindex  on;
//   }
//
//   # 2. autoindex OFF, no default index file
//   location /no-index-dir {
//       autoindex  off;
//   }
//
//   # 3. autoindex ON but an index.htm IS present inside the directory
//   location /auto-with-index {
//       autoindex  on;
//       index      index.htm;
//   }
//
// File-system setup:
//   mkdir -p <web-root>/autoindex-dir
//   echo "hello" > <web-root>/autoindex-dir/test-file.txt
//
//   mkdir -p <web-root>/no-index-dir
//   # — do NOT put any index file here —
//
//   mkdir -p <web-root>/auto-with-index
//   echo "<html><body>index</body></html>" > <web-root>/auto-with-index/index.htm
//
// CRITICAL — Content-Length requirement:
// ReadResponseFromServer terminates ONLY when it has read
// header_length + content_length bytes.  The autoindex-generated HTML body
// is dynamic in size, so the server MUST calculate its exact byte count and
// emit Content-Length in the response.  A missing Content-Length will cause
// every test in this suite to time out.
// ─────────────────────────────────────────────────────────────────────────────

AutoIndexTests::AutoIndexTests() : ATestList("Auto Index Tests")
{
	AddAllTests();
}

AutoIndexTests::~AutoIndexTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — autoindex ON, no index file  →  200 OK
// Targets: StaticFile (directory open), HTTPContext, Config autoindex flag
//
// When autoindex is enabled and there is no default index file in the
// directory, the server must generate an HTML directory listing and return
// 200 OK.  This is the baseline test — it proves the listing feature is
// active at all before any content or header assertions are made.
// ─────────────────────────────────────────────────────────────────────────────
void AutoIndexTests::AutoIndexEnabledTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "1 » Auto Index Enabled Test";
	testCase.description = "Test to check that the server returns 200 OK and "
	                       "generates a directory listing when autoindex is on "
	                       "and no default index file exists in the directory.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Trailing slash — requests the directory, not a file inside it
	testCase.request =
		"GET /autoindex-dir/ HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");

	testCase.configurationsForTestCase =
		"SETUP: 'location /autoindex-dir { autoindex on; }' with no index file. "
		"Create the directory and place at least one file inside it. "
		"CRITICAL: The generated listing body MUST be accompanied by a correct "
		"'Content-Length' header — without it the test reader will block until "
		"timeout. Calculate the byte count of the generated HTML before sending.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — autoindex OFF, no index file  →  403 Forbidden
// Targets: StaticFile (directory open), HTTPContext, Config autoindex flag
//
// When autoindex is disabled and there is no index file to serve, the server
// must NOT generate a listing.  The correct response is 403 Forbidden — the
// resource (the directory) exists but the server refuses to expose its
// contents.  This mirrors nginx's exact behaviour.
//
// 404 would be wrong (the directory exists).
// 200 with an empty body would be wrong (no listing should be generated).
// ─────────────────────────────────────────────────────────────────────────────
void AutoIndexTests::AutoIndexDisabledTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "2 » Auto Index Disabled Test";
	testCase.description = "Test to check that the server returns 403 Forbidden "
	                       "when autoindex is off and no default index file "
	                       "exists — the directory must not be listed.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /no-index-dir/ HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 403 Forbidden");

	testCase.configurationsForTestCase =
		"SETUP: 'location /no-index-dir { autoindex off; }' with NO index file. "
		"The directory must physically exist — if it is missing the server returns "
		"404 and this test fails for the wrong reason. "
		"The 403 response must include a body and 'Content-Length'. "
		"If the server returns 404 instead, the autoindex-disabled path is falling "
		"through to the 'file not found' branch rather than 'directory, no listing'.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — autoindex listing contains the filename
// Targets: StaticFile (directory read / listing generator), DefaultPages
//
// This is the most important test in the suite.  It exploits the fact that
// actServerResponse does response.find(expectedResponse) on the ENTIRE raw
// response — headers AND body — not just the status line.
//
// By setting expectedResponse to "test-file.txt" we assert that the file
// name actually appears somewhere in the response body, proving the server
// read the directory entries and serialised them into the HTML output.
//
// The test will fail if:
//   • The listing is generated but the filename is omitted.
//   • The listing HTML escapes the filename in a way that breaks the match
//     (e.g. "test&#8209;file.txt" — use plain ASCII names to avoid this).
//   • The server returns 200 with an empty body.
// ─────────────────────────────────────────────────────────────────────────────
void AutoIndexTests::AutoIndexShowsFilesTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "3 » Auto Index Shows Files Test";
	testCase.description = "Test to check that the autoindex-generated HTML "
	                       "body contains the name of a file that is known to "
	                       "exist inside the listed directory.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /autoindex-dir/ HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Substring match against the full response (headers + body).
	// "test-file.txt" must appear in the generated listing HTML.
	testCase.expectedResponse.push_back("test-file.txt");

	testCase.configurationsForTestCase =
		"SETUP: Same '/autoindex-dir/' as Test 1. "
		"A file named exactly 'test-file.txt' must exist inside the directory — "
		"create it with: echo \"hello\" > <web-root>/autoindex-dir/test-file.txt. "
		"The listing generator must emit the bare filename (not a URL-encoded "
		"or HTML-entity-encoded variant) so that this simple substring match "
		"finds it. If this test fails but Test 1 passes, the listing is being "
		"generated but the directory entries are not being serialised into the body.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — autoindex listing has correct content-type header
// Targets: StaticFile / listing generator, Repsense (MIME type assignment)
//
// The generated directory listing is an HTML document — the server must set
// content-type: text/html (with or without a charset parameter).
// Setting expectedResponse = "content-type: text/html" exploits the
// substring match: "content-type: text/html; charset=utf-8" also passes
// because it contains the required prefix.
//
// This test catches a bug where the listing is generated correctly but the
// server assigns no MIME type, a wrong MIME type (e.g. text/plain), or
// forgets the content-type header entirely because the response path for
// dynamically generated content is different from the static-file path.
// ─────────────────────────────────────────────────────────────────────────────
void AutoIndexTests::AutoIndexContentTypeTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "4 » Auto Index Content Type Test";
	testCase.description = "Test to check that the autoindex-generated directory "
	                       "listing response includes a 'content-type: text/html' "
	                       "header — confirming the server treats the listing as "
	                       "an HTML document, not a raw byte stream.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /autoindex-dir/ HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Matches "content-type: text/html" and also "content-type: text/html; charset=utf-8"
	testCase.expectedResponse.push_back("content-type: text/html");

	testCase.configurationsForTestCase =
		"SETUP: Same '/autoindex-dir/' as Test 1. "
		"When the server generates a dynamic listing body it must assign the "
		"MIME type 'text/html' in the content-type header, exactly as it would "
		"for a static .html file. A common bug is that the static-file MIME "
		"resolver is bypassed for generated content, resulting in a missing or "
		"'application/octet-stream' content-type. "
		"CRITICAL: Response must also include 'Content-Length'.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — autoindex ON but index file present  →  serves index file, not listing
// Targets: StaticFile (index file priority logic), Routing, Config (index directive)
//
// The subject specifies: "Default file to serve when the requested resource
// is a directory."  When an index file exists, it must take absolute priority
// over the autoindex listing — even when autoindex is enabled.
//
// This test re-uses the same mechanism as Test 3: expectedResponse is set to
// a string that ONLY appears inside the hand-crafted index.htm, not in any
// server-generated listing.  The chosen token is "AUTO-INDEX-PRIORITY-CHECK"
// — a unique sentinel embedded in the index.htm content during setup.
// If the server serves the listing instead of the file, this string will not
// be found and the test fails.
// ─────────────────────────────────────────────────────────────────────────────
void AutoIndexTests::AutoIndexWithIndexFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "5 » Auto Index With Index File Test";
	testCase.description = "Test to check that when autoindex is on AND a default "
	                       "index file exists, the server serves the index file "
	                       "and does NOT generate a directory listing — the index "
	                       "file must take absolute priority.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	testCase.request =
		"GET /auto-with-index/ HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// This token must be present verbatim inside auto-with-index/index.htm.
	// It will never appear in a server-generated directory listing, so a match
	// proves the index file was served, not the listing.
	testCase.expectedResponse.push_back("AUTO-INDEX-PRIORITY-CHECK");

	testCase.configurationsForTestCase =
		"SETUP: 'location /auto-with-index { autoindex on; index index.htm; }'. "
		"Create the index file with: "
		"echo '<html><body>AUTO-INDEX-PRIORITY-CHECK</body></html>' "
		"> <web-root>/auto-with-index/index.htm. "
		"The sentinel string 'AUTO-INDEX-PRIORITY-CHECK' must appear verbatim in "
		"the file — it is what this test looks for in the response body. "
		"If the test fails with a 200 OK but no match, the server is returning "
		"the autoindex listing (which never contains this string) instead of "
		"serving the index file. Fix the priority check: index file first, "
		"autoindex listing only as a fallback when no index file exists.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void AutoIndexTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Auto Index Enabled Test",         (void (ATestList::*)())&AutoIndexTests::AutoIndexEnabledTest) );
	_testFunctions.push_back( make_pair("Auto Index Disabled Test",        (void (ATestList::*)())&AutoIndexTests::AutoIndexDisabledTest) );
	_testFunctions.push_back( make_pair("Auto Index Shows Files Test",     (void (ATestList::*)())&AutoIndexTests::AutoIndexShowsFilesTest) );
	_testFunctions.push_back( make_pair("Auto Index Content Type Test",    (void (ATestList::*)())&AutoIndexTests::AutoIndexContentTypeTest) );
	_testFunctions.push_back( make_pair("Auto Index With Index File Test", (void (ATestList::*)())&AutoIndexTests::AutoIndexWithIndexFileTest) );
}
