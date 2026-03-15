#include "ChunkedTransferTests.hpp"
#include <sstream>
#include <iomanip>

// ─────────────────────────────────────────────────────────────────────────────
// WHAT IS CHUNKED TRANSFER ENCODING?
//
// When a client sends Transfer-Encoding: chunked, the request body is split
// into pieces.  Each piece is prefixed by its size in hexadecimal followed by
// \r\n, then the data, then another \r\n.  A zero-length chunk signals the end:
//
//   Transfer-Encoding: chunked
//   \r\n                           ← end of headers
//   <hex-size>\r\n                 ← chunk 1 size in hex
//   <data>\r\n                     ← chunk 1 data  (the \r\n is a separator)
//   <hex-size>\r\n                 ← chunk 2 size in hex
//   <data>\r\n                     ← chunk 2 data
//   0\r\n                          ← terminator chunk (size = 0)
//   \r\n                           ← blank line ends chunked body
//
// The server MUST:
//   1. Detect "Transfer-Encoding: chunked" in the request headers.
//   2. Parse each chunk-size line (hex digits + CRLF).
//   3. Read exactly chunk-size bytes of data, skip the trailing CRLF.
//   4. Repeat until the zero-length terminator chunk is found.
//   5. Deliver the reassembled body to the handler (POST / CGI) as if the
//      client had sent a plain body with Content-Length.
//
// The subject says: "for chunked requests, your server needs to un-chunk them,
// the CGI will expect EOF as the end of the body."
//
// ── HELPER ──────────────────────────────────────────────────────────────────
// chunkEncode(data) wraps a string in a single RFC-7230 chunk:
//   "<hex-size>\r\n<data>\r\n"
// Used internally by every test that builds a chunked body.
// ─────────────────────────────────────────────────────────────────────────────

// Returns a single chunk: "<hex-size>\r\n<data>\r\n"
static string chunkEncode(const string &data)
{
	ostringstream oss;
	oss << hex << data.size();          // size in lowercase hex, no prefix
	return oss.str() + "\r\n" + data + "\r\n";
}

// The terminator that ends every chunked body: "0\r\n\r\n"
static const string CHUNK_TERMINATOR = "0\r\n\r\n";

// Body size limit — must match 'client_max_body_size' in the server config
// for the /upload route.  Same constant as ClientBodySizeTests.
static const size_t BODY_LIMIT = 1024;

// ─────────────────────────────────────────────────────────────────────────────

ChunkedTransferTests::ChunkedTransferTests() : ATestList("Chunked Transfer Tests")
{
	AddAllTests();
}

ChunkedTransferTests::~ChunkedTransferTests()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — Single chunk + terminator  →  200 OK
// Targets: ClientRequest (chunked parser), POST handler, Routing
//
// The simplest valid chunked body: one chunk carrying "Hello, World!" (13
// bytes = 0xd in hex) followed by the zero terminator.
//
// Wire bytes sent after the headers:
//   "d\r\nHello, World!\r\n0\r\n\r\n"
//
// The server must:
//   1. Read "d\r\n"       → expect 13 bytes of data.
//   2. Read "Hello, World!\r\n"  → 13 bytes consumed, CRLF separator discarded.
//   3. Read "0\r\n\r\n"   → zero-length terminator, body complete.
//   4. Deliver "Hello, World!" (13 bytes) to the POST handler.
//   5. Return 200 OK with Content-Length in the response.
//
// Note: Transfer-Encoding and Content-Length are mutually exclusive in a
// request.  We send ONLY Transfer-Encoding: chunked — no Content-Length.
// ─────────────────────────────────────────────────────────────────────────────
void ChunkedTransferTests::ChunkedPostSimpleTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Chunked Post Simple Test";
	testCase.description = "Test to check that the server correctly parses a "
	                       "single-chunk chunked POST body (one data chunk + "
	                       "the zero terminator) and accepts the request with "
	                       "200 OK.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// One chunk: "Hello, World!" = 13 bytes = 0xd
	string chunkedBody = chunkEncode("Hello, World!") + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK||HTTP/1.1 201 Created");

	testCase.configurationsForTestCase =
		"SETUP: The '/upload' route must allow POST and have "
		"'client_max_body_size' >= 13 bytes. "
		"The raw chunked body sent is: \"d\\r\\nHello, World!\\r\\n0\\r\\n\\r\\n\". "
		"If this returns 400, the server is not recognising "
		"'Transfer-Encoding: chunked' and is treating the chunk-size line as "
		"part of the body. "
		"If it returns 411 Length Required, the server is incorrectly requiring "
		"Content-Length even when Transfer-Encoding is present. "
		"CRITICAL: The 200 response must include Content-Length so the test "
		"reader can exit its read loop.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — Three chunks reassembled into one body  →  200 OK
// Targets: ClientRequest (multi-chunk loop in the parser)
//
// Splits "Hello, World!" across three chunks of unequal sizes to stress the
// chunk-parsing loop:
//
//   Chunk 1: "Hello"  (5 bytes = 0x5)
//   Chunk 2: ", Wo"   (4 bytes = 0x4)
//   Chunk 3: "rld!"   (4 bytes = 0x4)
//   Terminator: 0\r\n\r\n
//
// The assembled body is "Hello, World!" — identical to Test 1.
// A 200 OK proves the parser correctly looped through all three chunks
// without stopping early after the first one.
//
// Unequal chunk sizes are intentional — some naive parsers work when all
// chunks are the same size but fail on irregular splits.
// ─────────────────────────────────────────────────────────────────────────────
void ChunkedTransferTests::ChunkedPostMultiChunkTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Chunked Post Multi Chunk Test";
	testCase.description = "Test to check that the server correctly loops through "
	                       "multiple chunks of unequal sizes, reassembles all of "
	                       "them into a single body, and accepts the request with "
	                       "200 OK.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Three unequal chunks that together form "Hello, World!"
	string chunkedBody =
		chunkEncode("Hello")   // 5 bytes → "5\r\nHello\r\n"
		+ chunkEncode(", Wo")  // 4 bytes → "4\r\n, Wo\r\n"
		+ chunkEncode("rld!")  // 4 bytes → "4\r\nrld!\r\n"
		+ CHUNK_TERMINATOR;    // "0\r\n\r\n"

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK||HTTP/1.1 201 Created");

	testCase.configurationsForTestCase =
		"SETUP: Same '/upload' route as Test 1. "
		"Raw body sent: \"5\\r\\nHello\\r\\n4\\r\\n, Wo\\r\\n4\\r\\nrld!\\r\\n0\\r\\n\\r\\n\". "
		"If Test 1 passed but this fails, the chunk parser exits after the first "
		"chunk instead of looping — check that the parse loop continues until the "
		"zero-length terminator, not until any single chunk is consumed. "
		"If this returns 400, the hex size parsing is failing on single-digit "
		"values for intermediate chunks — verify the hex-to-int conversion handles "
		"any number of hex digits correctly.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — Chunked body delivered to CGI stdin intact
// Targets: ClientRequest (un-chunker), CGI (stdin pipe), CGIPipe,
//          AMethod (POST→CGI dispatch)
//
// This is the only test the subject explicitly motivates:
// "for chunked requests, your server needs to un-chunk them, the CGI will
//  expect EOF as the end of the body."
//
// The test sends a two-chunk body that assembles to "CGI-CHUNKED-INPUT".
// The echo_post.py CGI script (from CGITests setup) reads CONTENT_LENGTH bytes
// from stdin and echoes them back.  A match on "CGI-CHUNKED-INPUT" proves:
//   1. The server un-chunked the body before writing to the pipe.
//   2. CONTENT_LENGTH was set to the assembled size (17 bytes), not the
//      size of the raw chunked wire bytes.
//   3. The CGI received a clean body with no chunk-size lines inside it.
//
// Wire format (two chunks):
//   "9\r\nCGI-CHUNK\r\ne\r\nED-INPUT\r\n   wait — let me recount"
//
// "CGI-CHUNKED-INPUT" = 17 bytes
//   Split: "CGI-CHUNK" (9 bytes = 0x9) + "ED-INPUT" (8 bytes = 0x8)
//   → "9\r\nCGI-CHUNK\r\n8\r\nED-INPUT\r\n0\r\n\r\n"
//
// Reuses the echo_post.py script from CGITests.  If CGITests are not yet
// set up, create echo_post.py as described in CGITests configFileData.
// ─────────────────────────────────────────────────────────────────────────────
void ChunkedTransferTests::ChunkedBodyEchoedByCgiTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Chunked Body Echoed By CGI Test";
	testCase.description = "Test to check that the server un-chunks the request "
	                       "body BEFORE writing it to the CGI stdin pipe — the "
	                       "CGI must receive a clean assembled body with no chunk "
	                       "framing, proven by echoing it back and matching the "
	                       "original string.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// "CGI-CHUNKED-INPUT" split across two chunks
	// Chunk 1: "CGI-CHUNK" = 9 bytes = 0x9
	// Chunk 2: "ED-INPUT"  = 8 bytes = 0x8
	// Assembled: "CGI-CHUNKED-INPUT" = 17 bytes
	string chunk1      = "CGI-CHUNK";  // 9 bytes
	string chunk2      = "ED-INPUT";   // 8 bytes
	string assembled   = chunk1 + chunk2;   // "CGI-CHUNKED-INPUT"
	string chunkedBody = chunkEncode(chunk1) + chunkEncode(chunk2) + CHUNK_TERMINATOR;

	testCase.request =
		"POST /cgi-bin/echo_post.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	// The echoed assembled body must appear in the response
	testCase.expectedResponse.push_back("CGI-CHUNKED-INPUT");

	testCase.configurationsForTestCase =
		"SETUP: Requires echo_post.py from CGITests — see CGITests configFileData "
		"for the script content. "
		"Raw chunked body sent: \"9\\r\\nCGI-CHUNK\\r\\n8\\r\\nED-INPUT\\r\\n0\\r\\n\\r\\n\". "
		"The CGI must receive 'CGI-CHUNKED-INPUT' (17 bytes) on stdin, NOT the "
		"raw chunked wire bytes. "
		"IMPORTANT: The server must set CONTENT_LENGTH=17 in the CGI environment "
		"(the assembled size), not the wire byte count of the chunked body. "
		"If the CGI echoes chunk-size lines like '9' or '8', the server piped the "
		"raw chunked bytes directly to stdin without un-chunking. "
		"If the body is empty, CONTENT_LENGTH was not set or was set to 0.";

	testCase.timeout = -1;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — Assembled chunked body exceeds client_max_body_size  →  413
// Targets: ClientRequest (size check on assembled size, not wire size),
//          Config (client_max_body_size enforcement)
//
// Sends BODY_LIMIT + 1 bytes of actual data split across two chunks.
// The assembled body is 1025 bytes — one byte over the 1024-byte limit.
//
// This test is more subtle than PostOneByteOverLimitTest in ClientBodySizeTests:
// the wire representation is LARGER than the assembled body (chunk-size lines
// add overhead), so the server must enforce the limit against the ASSEMBLED
// byte count, not the raw bytes received on the socket.
//
// Chunk sizes:
//   Chunk 1: 512 bytes of 'A' = 0x200
//   Chunk 2: 513 bytes of 'A' = 0x201
//   Assembled: 1025 bytes = BODY_LIMIT + 1  →  must trigger 413
// ─────────────────────────────────────────────────────────────────────────────
void ChunkedTransferTests::ChunkedBodySizeLimitTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Chunked Body Size Limit Test";
	testCase.description = "Test to check that the server enforces client_max_body_size "
	                       "against the ASSEMBLED chunked body size, not the raw wire "
	                       "byte count — a body that assembles to limit+1 bytes must "
	                       "be rejected with 413 Content Too Large.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Two chunks whose assembled total is BODY_LIMIT + 1 (1025 bytes)
	string chunk1      = string(512, 'A');          // 0x200
	string chunk2      = string(BODY_LIMIT + 1 - 512, 'A');  // 0x201  (513 bytes)
	string chunkedBody = chunkEncode(chunk1) + chunkEncode(chunk2) + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 413 Content Too Large||HTTP/1.1 413 Payload Too Large");

	testCase.configurationsForTestCase =
		"SETUP: '/upload' route with 'client_max_body_size 1024'. "
		"This test sends 1025 assembled bytes split into two chunks. "
		"The server should ideally reject as soon as the running assembled total "
		"exceeds the limit — it should not buffer all 1025 bytes first. "
		"If this returns 200 OK, the size check is running against the raw socket "
		"bytes (which include chunk-size lines) rather than the assembled body — "
		"fix the check to accumulate chunk data sizes, not socket read sizes. "
		"CRITICAL: The 413 response must include Content-Length.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — Zero-length chunked body (terminator only)  →  200 OK
// Targets: ClientRequest (edge case: empty chunked body)
//
// A valid chunked POST with no data at all — only the zero terminator chunk.
// This represents a POST with an intentionally empty body, which is legal.
//
// Wire bytes after the headers:
//   "0\r\n\r\n"
//
// The server must:
//   1. Read "0\r\n"  → zero-length chunk: body is complete, size = 0.
//   2. Read "\r\n"   → trailing blank line, chunked body fully consumed.
//   3. Deliver an empty body (0 bytes) to the POST handler.
//   4. Return 200 OK.
//
// This is the boundary case between "has a body" and "body is empty".
// A common bug is the parser treating the zero terminator as an error because
// it reads zero bytes and interprets that as a broken connection rather than
// a valid end-of-chunked-body signal.
// ─────────────────────────────────────────────────────────────────────────────
void ChunkedTransferTests::ChunkedEmptyBodyTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "Chunked Empty Body Test";
	testCase.description = "Test to check that the server correctly handles a "
	                       "chunked POST with a zero-length body — only the "
	                       "terminator chunk is sent, which is valid HTTP/1.1. "
	                       "The server must not treat the zero-size chunk as a "
	                       "broken connection and must return 200 OK.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Only the terminator — no data chunks at all
	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ CHUNK_TERMINATOR;   // "0\r\n\r\n"

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK||HTTP/1.1 201 Created");

	testCase.configurationsForTestCase =
		"SETUP: Same '/upload' route as Tests 1 and 2. "
		"The raw body sent after the headers is exactly: \"0\\r\\n\\r\\n\". "
		"If this returns 400, the chunk parser is treating a zero-size chunk as "
		"an error — the zero terminator is perfectly valid and signals end-of-body. "
		"If this returns 411 Length Required, the server is requiring Content-Length "
		"even when Transfer-Encoding is present, which violates RFC 7230 §3.3. "
		"If the test times out, the parser is waiting for more data after reading "
		"the zero-size terminator instead of signalling body-complete. "
		"CRITICAL: The 200 response must include Content-Length.";

	testCase.timeout = 3000;

	RunTestCase(testCase);
}

void ChunkedTransferTests::ChunkedMultipartTest()
{
    // arrange
    TestCase testCase;
    testCase.name        = "Chunked Multipart File Upload";
    testCase.description = "Test to check that the server correctly de-chunks a body, "
                           "and then successfully parses the resulting multipart/form-data "
                           "to extract the filename and file content.";
    testCase.port        = "1025";
    testCase.host        = "localhost";

    // 1. The raw, unchunked multipart payload we want the server to extract.
    // The total length of this string is exactly 147 bytes.
    std::string multipartData = 
        "--MyBoundary\r\n"
        "Content-Disposition: form-data; name=\"upload\"; filename=\"test.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hello from a chunk!\r\n"
        "--MyBoundary--\r\n";
    
    // 2. We wrap it in chunked format. 147 in hexadecimal is "93".
    std::string chunkedBody = 
        "93\r\n"
        + multipartData + 
		"\r\n"
        "0\r\n\r\n";

    // 3. Assemble the full request
    testCase.request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        // Notice the boundary defined here matches the one in the body
        "Content-Type: multipart/form-data; boundary=MyBoundary\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        + chunkedBody;

    // Accepting 201 Created (best practice) or 200 OK
    testCase.expectedResponse.push_back("HTTP/1.1 201 Created||HTTP/1.1 200 OK");

    testCase.configurationsForTestCase =
        "SETUP: Same '/upload' route. "
        "The server must first de-chunk the payload. "
        "Then, it must recognize the 'multipart/form-data' Content-Type, "
        "parse the boundary, and extract the file named 'test.txt' "
        "containing the text 'Hello from a chunk!'.";

    testCase.timeout = 3000;

    RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
void ChunkedTransferTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Chunked Post Simple Test",          (void (ATestList::*)())&ChunkedTransferTests::ChunkedPostSimpleTest) );
	_testFunctions.push_back( make_pair("Chunked Post Multi Chunk Test",     (void (ATestList::*)())&ChunkedTransferTests::ChunkedPostMultiChunkTest) );
	_testFunctions.push_back( make_pair("Chunked Body Echoed By CGI Test",   (void (ATestList::*)())&ChunkedTransferTests::ChunkedBodyEchoedByCgiTest) );
	_testFunctions.push_back( make_pair("Chunked Body Size Limit Test",      (void (ATestList::*)())&ChunkedTransferTests::ChunkedBodySizeLimitTest) );
	_testFunctions.push_back( make_pair("Chunked Empty Body Test",           (void (ATestList::*)())&ChunkedTransferTests::ChunkedEmptyBodyTest) );
	_testFunctions.push_back( make_pair("Chunked Multipart File Upload Test", (void (ATestList::*)())&ChunkedTransferTests::ChunkedMultipartTest) );
}
