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
	testCase.name        = "1 » Chunked Post Simple Test";
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

	testCase.timeout = 10000;

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
	testCase.name        = "2 » Chunked Post Multi Chunk Test";
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

	testCase.timeout = 10000;

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
	testCase.name        = "3 » Chunked Body Echoed By CGI Test";
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

	testCase.timeout = 10000; // No timeout

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
	testCase.name        = "4 » Chunked Body Size Limit Test";
	testCase.description = "Test to check that the server enforces client_max_body_size "
	                       "against the ASSEMBLED chunked body size, not the raw wire "
	                       "byte count — a body that assembles to limit+1 bytes must "
	                       "be rejected with 413 Content Too Large.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Two chunks whose assembled total is BODY_LIMIT + 1 (2049 bytes)
	string chunk1      = string(1024, 'A');          // 0x400
	string chunk2      = string(1025, 'A');  // 0x401  (1025 bytes)
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

	testCase.timeout = 10000;

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
	testCase.name        = "5 » Chunked Empty Body Test";
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

	testCase.timeout = 10000;

	RunTestCase(testCase);
}

void ChunkedTransferTests::ChunkedMultipartTest()
{
    // arrange
    TestCase testCase;
    testCase.name        = "6 » Chunked Multipart File Upload";
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

    testCase.timeout = -1;

    RunTestCase(testCase);
}

void ChunkedTransferTests::BoundarySplitAcrossChunksTest()
{
	// arrange
	TestCase postCase;
	postCase.name = "7 » Chunked Multipart Boundary Split Across Chunks";
	postCase.description = "Send a multipart/form-data body using chunked transfer encoding where the boundary delimiter is split across chunk boundaries. After POST (201/200) perform GET /upload/a.txt to verify the file contents.";
	postCase.port = "1025";
	postCase.host = "localhost";

	// Build multipart payload but split the initial boundary across chunks
	const string boundary = "MyBoundary";

	// Compose segments so the boundary string is split between chunks
	string seg1 = "--MyBou"; // partial boundary (will end a chunk)
	string seg2 = string("ndary\r\n") +
				  "Content-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n" +
				  "Content-Type: text/plain\r\n\r\n" +
				  "Text for test\r\n" +
				  "--" + boundary + "--\r\n"; // rest of multipart including closing

	// Wrap the segments into chunked encoding (two chunks, boundary split across them)
	string chunkedBody = chunkEncode(seg1) + chunkEncode(seg2) + CHUNK_TERMINATOR;

	postCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=MyBoundary\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	postCase.expectedResponse.push_back("HTTP/1.1 201 Created||HTTP/1.1 200 OK");
	postCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data and save uploaded files by filename. This test splits the multipart boundary across chunk boundaries to verify the server de-chunks before parsing.";
	postCase.timeout = -1;

	// Perform POST
	RunTestCase(postCase);
}

void ChunkedTransferTests::MultiFileUploadTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "8 » Chunked Multipart Multi-File Upload Test";
	testCase.description = "Send a multipart/form-data request with two file parts wrapped in chunked transfer encoding. Server should accept and return 201 Created (or 200 OK). Do NOT attempt to GET uploaded files because server appends a random suffix to filenames.";
	testCase.port = "1025";
	testCase.host = "localhost";

	const string boundary = "MultiPartBound";

	// Build multipart body with two file parts
	string part1 = "--" + boundary + "\r\n";
	part1 += "Content-Disposition: form-data; name=\"file1\"; filename=\"file1.txt\"\r\n";
	part1 += "Content-Type: text/plain\r\n\r\n";
	part1 += "Content of file one\r\n";

	string part2 = "--" + boundary + "\r\n";
	part2 += "Content-Disposition: form-data; name=\"file2\"; filename=\"file2.txt\"\r\n";
	part2 += "Content-Type: text/plain\r\n\r\n";
	part2 += "Content of file two\r\n";

	string closing = "--" + boundary + "--\r\n";

	// Wrap into chunked body with multiple small chunks to exercise parser
	string chunkedBody = chunkEncode(part1) + chunkEncode(part2) + chunkEncode(closing) + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created||HTTP/1.1 200 OK");
	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data. Server may rename files by appending a random suffix; this test only verifies that the POST succeeds (201/200).";
	testCase.timeout = 10000;

	RunTestCase(testCase);
}

void ChunkedTransferTests::MultipleFieldsTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "9 » Chunked Multipart Multiple Fields Test";
	testCase.description = "Send multipart/form-data with several text fields and one file wrapped in chunked transfer encoding. Server should de-chunk and parse all fields, accepting the upload (201/200).";
	testCase.port = "1025";
	testCase.host = "localhost";

	const string boundary = "FieldsBound";

	// Build multipart parts: two text fields and one file part
	string part1 = "--" + boundary + "\r\n";
	part1 += "Content-Disposition: form-data; name=\"username\"\r\n\r\n";
	part1 += "tester\r\n";

	string part2 = "--" + boundary + "\r\n";
	part2 += "Content-Disposition: form-data; name=\"description\"\r\n\r\n";
	part2 += "Multiple fields test\r\n";

	string part3 = "--" + boundary + "\r\n";
	part3 += "Content-Disposition: form-data; name=\"file\"; filename=\"note.txt\"\r\n";
	part3 += "Content-Type: text/plain\r\n\r\n";
	part3 += "Note contents\r\n";

	string closing = "--" + boundary + "--\r\n";

	// Wrap into chunked body using several chunks
	string chunkedBody = chunkEncode(part1) + chunkEncode(part2) + chunkEncode(part3) + chunkEncode(closing) + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created||HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data. This test uploads two text fields and one file in a single multipart request wrapped in chunked encoding. The server must de-chunk the body then parse all fields. The file 'note.txt' may be renamed by the server; this test only asserts the POST succeeds.";

	testCase.timeout = -1;
	
	RunTestCase(testCase);

}


void ChunkedTransferTests::MalformedBoundaryTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "10 » Chunked Multipart Malformed Boundary Test";
	testCase.description = "Send a chunked multipart/form-data where the declared boundary does not match the multipart delimiters in the body (malformed). Server should reject with 400 Bad Request or similar.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Declared boundary is 'GoodBound' but body uses '--BadBound' markers -> mismatch
	const string declaredBoundary = "GoodBound";
	const string bodyBoundary = "BadBound";

	string malformedMultipart =
		"--" + bodyBoundary + "\r\n"
		"Content-Disposition: form-data; name=\"field\"\r\n\r\n"
		"value\r\n"
		"--" + bodyBoundary + "--\r\n";

	// Wrap in chunked encoding
	string chunkedBody = chunkEncode(malformedMultipart) + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + declaredBoundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	// Expect server to error on malformed multipart boundary
	testCase.expectedResponse.push_back("HTTP/1.1 400 Bad Request||HTTP/1.1 400");

	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data. This test sends a body whose delimiters use a different boundary than the one declared in the Content-Type header. The server must de-chunk first, then detect the mismatch and respond with 400 Bad Request.";

	testCase.timeout = -1;

	RunTestCase(testCase);

}


void ChunkedTransferTests::MissingClosingBoundaryTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "11 » Chunked Multipart Missing Closing Boundary Test";
	testCase.description = "Send a chunked multipart/form-data where the final closing boundary is missing. Server should detect the malformed multipart and return 400 Bad Request.";
	testCase.port = "1025";
	testCase.host = "localhost";

	const string boundary = "NoCloseBound";

	// Multipart body that never sends the closing '--boundary--' delimiter
	string multipart =
		"--" + boundary + "\r\n"
		"Content-Disposition: form-data; name=\"field\"\r\n\r\n"
		"value without closing\r\n";

	// Note: intentionally omit the closing '--NoCloseBound--\r\n'

	// Wrap in chunked encoding
	string chunkedBody = chunkEncode(multipart) + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 400 Bad Request||HTTP/1.1 400");

	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data. This test omits the required closing boundary token; server must detect the malformed multipart after de-chunking and respond with 400.";

	testCase.timeout = 10000;

	RunTestCase(testCase);

}


void ChunkedTransferTests::BinaryDataTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "12 » Chunked Binary Data Test";
	testCase.description = "Send multipart/form-data containing a binary file part wrapped in chunked transfer encoding. Server should de-chunk and accept the upload (201/200).";
	testCase.port = "1025";
	testCase.host = "localhost";

	const string boundary = "BinBound";

	// Build a single file part with binary content (includes NUL and non-ASCII bytes)
	string part = "--" + boundary + "\r\n";
	part += "Content-Disposition: form-data; name=\"file\"; filename=\"binary.bin\"\r\n";
	part += "Content-Type: application/octet-stream\r\n\r\n";

	// Binary payload: bytes 0x00,0x01,0x02,0xff,0xfe then ASCII 'Binary', NUL, 'Data'
	string binaryData = string("\x00\x01\x02\xff\xfe,Binary\x00Data", 16);
	part += binaryData + "\r\n";

	part += "--" + boundary + "--\r\n";

	// Wrap into chunked body
	string chunkedBody = chunkEncode(part) + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created||HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data. This test sends a binary file part (contains NUL and 0xFF bytes) wrapped in chunked encoding. The server must de-chunk then parse the multipart and accept the upload. The test only asserts the POST succeeded.";

	testCase.timeout = 10000;

	RunTestCase(testCase);

}


void ChunkedTransferTests::LargeFileStreamTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "13 » Chunked Large File Stream Test";
	testCase.description = "Send a large file (just under body limit) as a multipart/form-data part using many small chunks to simulate streaming upload. Server should de-chunk, parse, and accept (201/200).";
	testCase.port = "1025";
	testCase.host = "localhost";

	const string boundary = "LargeStreamBound";
	const size_t fileSize = BODY_LIMIT - 10; // Just under the limit

	// Build a large file part
	string part = "--" + boundary + "\r\n";
	part += "Content-Disposition: form-data; name=\"file\"; filename=\"bigfile.bin\"\r\n";
	part += "Content-Type: application/octet-stream\r\n\r\n";
	part += string(fileSize, 'X'); // Large payload
	part += "\r\n--" + boundary + "--\r\n";

	// Split into many small chunks (simulate streaming)
	string chunkedBody;
	size_t pos = 0;
	size_t chunkSize = 32; // Small chunk size
	while (pos < part.size()) {
		size_t len = std::min(chunkSize, part.size() - pos);
		chunkedBody += chunkEncode(part.substr(pos, len));
		pos += len;
	}
	chunkedBody += CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created||HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data and allow large uploads. This test sends a large file part split into many small chunks to simulate a streaming upload. The server must de-chunk, parse, and accept the upload.";

	testCase.timeout = 10000;

	RunTestCase(testCase);
}

void ChunkedTransferTests::SlowUploadByteByByteTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "14 » Chunked Slow Upload Byte By Byte Test";
	testCase.description = "Send a small multipart/form-data upload using chunked transfer encoding, but send the request body one byte at a time with a 10s delay between each byte. Server should handle slow uploads and accept (201/200).";
	testCase.port = "1025";
	testCase.host = "localhost";

	const string boundary = "SlowByteBound";

	// Small file part
	string part = "--" + boundary + "\r\n";
	part += "Content-Disposition: form-data; name=\"file\"; filename=\"slow.txt\"\r\n";
	part += "Content-Type: text/plain\r\n\r\n";
	part += "slow upload\r\n";
	part += "--" + boundary + "--\r\n";

	// Wrap in chunked encoding (single chunk for simplicity)
	string chunkedBody = chunkEncode(part) + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created||HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data. This test sends a small file part using chunked encoding, but the body is sent one byte at a time with a 10s delay between bytes. The server must tolerate slow uploads and accept the request.";

	testCase.maxSend = 1; // Send one byte per send
	testCase.sleepTime = 50000; // Sleep 10s after each send
	testCase.timeout = -1;

	RunTestCase(testCase);
}

void ChunkedTransferTests::PrematureEOFTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "15 » Chunked Premature EOF Test";
	testCase.description = "Send a chunked multipart/form-data request that ends prematurely (missing chunk data or terminator). Server should detect incomplete body and return 400 Bad Request.";
	testCase.port = "1025";
	testCase.host = "localhost";

	const string boundary = "PremEOFBound";

	// Build a valid multipart part
	string part = "--" + boundary + "\r\n";
	part += "Content-Disposition: form-data; name=\"file\"; filename=\"prem.txt\"\r\n";
	part += "Content-Type: text/plain\r\n\r\n";
	part += "premature eof\r\n";
	part += "--" + boundary + "--\r\n";

	// Intentionally truncate the chunked body (omit last 5 bytes)
	string fullChunked = chunkEncode(part) + CHUNK_TERMINATOR;
	string truncated = fullChunked.substr(0, fullChunked.size() - 5); // Remove last 5 bytes

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ truncated;

	testCase.expectedResponse.push_back("HTTP/1.1 400 Bad Request||HTTP/1.1 400");

	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data. This test sends a chunked body that ends before the chunked terminator or chunk data is complete. The server must detect the incomplete body and return 400.";

	testCase.timeout = -1;

	RunTestCase(testCase);
}

void ChunkedTransferTests::ChunkExtensionsAndTrailersTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "16 » Chunked Extensions and Trailers Test";
	testCase.description = "Send a chunked multipart/form-data request using chunk extensions and a trailer header. Server must ignore chunk extensions and process trailers per RFC 7230.";
	testCase.port = "1025";
	testCase.host = "localhost";

	const string boundary = "ExtTrailerBound";

	// Build a simple multipart part
	string part = "--" + boundary + "\r\n";
	part += "Content-Disposition: form-data; name=\"file\"; filename=\"ext.txt\"\r\n";
	part += "Content-Type: text/plain\r\n\r\n";
	part += "chunk extensions\r\n";
	part += "--" + boundary + "--\r\n";

	// Manually build chunked body with chunk extensions and a trailer
	std::ostringstream oss;
	oss << std::hex << part.size();
	string chunkSizeHex = oss.str();
	string chunkedBody = chunkSizeHex + ";foo=bar;baz\r\n" + part + "\r\n"; // chunk extension
	chunkedBody += "0\r\n"; // terminator chunk
	chunkedBody += "X-Trailer-Header: trailer-value\r\n"; // trailer header
	chunkedBody += "\r\n"; // end of trailers

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Trailer: X-Trailer-Header\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created||HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "SETUP: '/upload' route must accept multipart/form-data. This test sends a chunked body with chunk extensions and a trailer header. The server must ignore chunk extensions and process trailers per RFC 7230. The test only asserts the POST succeeded.";

	testCase.timeout = 10000;

	RunTestCase(testCase);
}


// ─────────────────────────────────────────────────────────────────────────────
// ChunkSizeEdgeCasesTest — non-canonical but RFC-legal chunk-size formats
// Targets: ClientRequest (hex parser robustness)
//
// RFC 7230 §4.1 says the chunk-size is 1*HEXDIG.  It does NOT forbid:
//   • Uppercase hex digits  (e.g. "A" instead of "a" for 10 bytes)
//   • Leading zeros         (e.g. "0000d" instead of "d" for 13 bytes)
//
// A strict but correct parser must accept both forms.  This test sends
// three chunks:
//   Chunk 1: size "0000D" (uppercase, leading zeros) → 13 bytes "Hello, World!"
//   Chunk 2: size "A"     (uppercase, no leading zeros) → 10 bytes "0123456789"
//   Chunk 3: size "1"     (single byte) → 1 byte "!"
//
// Assembled body: "Hello, World!0123456789!" (24 bytes)
//
// A 200/201 proves the hex parser does not reject legal but non-minimal
// chunk-size representations.
// ─────────────────────────────────────────────────────────────────────────────
void ChunkedTransferTests::ChunkSizeEdgeCasesTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "17 » Chunk Size Edge Cases Test";
	testCase.description = "Test that the server accepts non-canonical but RFC-legal "
	                       "chunk-size representations: leading zeros (\"0000D\") and "
	                       "uppercase hex digits (\"A\"), plus a minimal single-byte "
	                       "chunk (\"1\"). A 200/201 proves the hex parser is not "
	                       "limited to lowercase or zero-padded-free representations.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	// Chunk 1: "0000D\r\n" → 13 bytes (0x0000D = 13), uppercase + leading zeros
	const string data1 = "Hello, World!"; // 13 bytes = 0xD
	string chunk1 = "0000D\r\n" + data1 + "\r\n";

	// Chunk 2: "A\r\n" → 10 bytes (0xA = 10), uppercase single digit
	const string data2 = "0123456789"; // 10 bytes = 0xA
	string chunk2 = "A\r\n" + data2 + "\r\n";

	// Chunk 3: "1\r\n" → 1 byte, minimal single-byte chunk
	const string data3 = "!"; // 1 byte = 0x1
	string chunk3 = "1\r\n" + data3 + "\r\n";

	string chunkedBody = chunk1 + chunk2 + chunk3 + CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: text/plain\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK||HTTP/1.1 201 Created");

	testCase.configurationsForTestCase =
		"SETUP: Same '/upload' route as prior tests, POST enabled. "
		"Raw chunked body: \"0000D\\r\\nHello, World!\\r\\n\" "
		"+ \"A\\r\\n0123456789\\r\\n\" "
		"+ \"1\\r\\n!\\r\\n\" "
		"+ \"0\\r\\n\\r\\n\". "
		"Assembled body is \"Hello, World!0123456789!\" (24 bytes). "
		"If this returns 400, the chunk-size parser is rejecting leading zeros or "
		"uppercase hex digits — RFC 7230 §4.1 permits 1*HEXDIG with no restriction "
		"on case or leading zeros; fix the parser to accept both. "
		"If Chunk 2 ('A') is misread as 0 (treated as a terminator), the parser "
		"is doing a decimal atoi() instead of strtol(base 16).";

	testCase.timeout = 10000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────────────────────────────────────
// ContentDispositionAndFilenameTest — filename parameter parsing in multipart
// Targets: ClientRequest (multipart parser, Content-Disposition header parser)
//
// Tests that the server correctly handles three tricky filename values that
// are all legal per RFC 2183 / RFC 6266:
//
//   Part 1 — filename with spaces (must be quoted):
//             filename="my upload.txt"
//
//   Part 2 — filename with special characters:
//             filename="résumé_(final).pdf"
//
//   Part 3 — path-traversal attempt in filename:
//             filename="../evil.txt"
//             The server MUST NOT write to the path "../evil.txt".
//             It should either strip the path component, sanitise to
//             "evil.txt", or reject the request with 400.
//             This test accepts both 200/201 (sanitised) and 400 (rejected).
//
// The entire multipart body is wrapped in chunked transfer encoding.
// A successful response proves the server parses quoted filenames with
// spaces and special characters without misreading the CRLF boundaries
// of the part headers.
// ─────────────────────────────────────────────────────────────────────────────
void ChunkedTransferTests::ContentDispositionAndFilenameTest()
{
	// arrange
	TestCase testCase;
	testCase.name        = "18 » Content Disposition And Filename Test";
	testCase.description = "Test that the server correctly parses the filename "
	                       "parameter of Content-Disposition in multipart parts: "
	                       "quoted filename with spaces, filename with special "
	                       "characters, and a path-traversal filename "
	                       "(\"../evil.txt\"). The last part must be sanitised or "
	                       "rejected — writing to the literal path is not acceptable.";
	testCase.port        = "1025";
	testCase.host        = "localhost";

	const string boundary = "CDFilenameBound";

	// Part 1 — quoted filename with an embedded space
	string part1 =
		"--" + boundary + "\r\n"
		"Content-Disposition: form-data; name=\"file1\"; filename=\"my upload.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"contents of file with spaces in name\r\n";

	// Part 2 — filename with special characters (accents + parentheses)
	string part2 =
		"--" + boundary + "\r\n"
		"Content-Disposition: form-data; name=\"file2\"; filename=\"resume_(final).txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"resume content\r\n";

	// Part 3 — path-traversal attempt: server must sanitise or reject
	string part3 =
		"--" + boundary + "\r\n"
		"Content-Disposition: form-data; name=\"file3\"; filename=\"../evil.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"path traversal payload\r\n";

	string closing = "--" + boundary + "--\r\n";

	// Encode each part as its own chunk to stress the interplay between
	// chunk boundaries and multipart header parsing
	string chunkedBody =
		chunkEncode(part1) +
		chunkEncode(part2) +
		chunkEncode(part3) +
		chunkEncode(closing) +
		CHUNK_TERMINATOR;

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		+ chunkedBody;

	// 200/201: server accepted + sanitised the path-traversal filename.
	// 400: server rejected the path-traversal outright. Both are correct.
	testCase.expectedResponse.push_back(
		"HTTP/1.1 201 Created||HTTP/1.1 200 OK||HTTP/1.1 400 Bad Request");

	testCase.configurationsForTestCase =
		"SETUP: '/upload' route must accept multipart/form-data. "
		"Part 1 has filename=\"my upload.txt\" (space inside quotes) — "
		"a parser that stops at the space will misread the name as \"my\" and "
		"break the header boundary detection for the rest of the part. "
		"Part 2 has filename=\"resume_(final).txt\" — parentheses and underscores "
		"are legal token characters but some parsers choke on them. "
		"Part 3 has filename=\"../evil.txt\" — CRITICAL security check. "
		"The server must NEVER write to '../evil.txt' literally. "
		"Acceptable outcomes: strip path components and save as 'evil.txt', "
		"or return 400. If the server silently saves to a path outside the upload "
		"directory this is a path-traversal vulnerability. "
		"If Parts 1 or 2 cause a 400, the Content-Disposition parser is not "
		"correctly handling quoted-string values that contain spaces or special chars.";

	testCase.timeout = 10000;

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
	_testFunctions.push_back( make_pair("Boundary Split Across Chunks Test", (void (ATestList::*)())&ChunkedTransferTests::BoundarySplitAcrossChunksTest) );
	_testFunctions.push_back( make_pair("Multi-File Upload Test", (void (ATestList::*)())&ChunkedTransferTests::MultiFileUploadTest) );
	_testFunctions.push_back( make_pair("Multiple Fields Test", (void (ATestList::*)())&ChunkedTransferTests::MultipleFieldsTest) );
	_testFunctions.push_back( make_pair("Malformed Boundary Test", (void (ATestList::*)())&ChunkedTransferTests::MalformedBoundaryTest) );
	_testFunctions.push_back( make_pair("Missing Closing Boundary Test", (void (ATestList::*)())&ChunkedTransferTests::MissingClosingBoundaryTest) );
	_testFunctions.push_back( make_pair("Chunked Binary Data Test", (void (ATestList::*)())&ChunkedTransferTests::BinaryDataTest) );
	_testFunctions.push_back( make_pair("Chunked Large File Stream Test", (void (ATestList::*)())&ChunkedTransferTests::LargeFileStreamTest) );
	_testFunctions.push_back( make_pair("Chunked Slow Upload Byte By Byte Test", (void (ATestList::*)())&ChunkedTransferTests::SlowUploadByteByByteTest) );
	_testFunctions.push_back( make_pair("Chunked Premature EOF Test", (void (ATestList::*)())&ChunkedTransferTests::PrematureEOFTest) );
	_testFunctions.push_back( make_pair("Chunked Extensions and Trailers Test", (void (ATestList::*)())&ChunkedTransferTests::ChunkExtensionsAndTrailersTest) );
	_testFunctions.push_back( make_pair("Chunk Size Edge Cases Test",            (void (ATestList::*)())&ChunkedTransferTests::ChunkSizeEdgeCasesTest) );
	_testFunctions.push_back( make_pair("Content Disposition And Filename Test", (void (ATestList::*)())&ChunkedTransferTests::ContentDispositionAndFilenameTest) );
}


