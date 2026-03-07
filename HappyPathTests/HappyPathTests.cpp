#include "HappyPathTests.hpp"

HappyPathTests::HappyPathTests() : ATestList("Happy Path Tests")
{
	AddAllTests();
}

HappyPathTests::~HappyPathTests()
{
}

void HappyPathTests::GetIndexTest()
{
	// arange
	TestCase testCase;
	testCase.name = "Get Index Test";
	testCase.description = "Test to check if the server returns the correct index page";
	testCase.port = "1025";
	testCase.host = "localhost";
	testCase.request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 200 OK";
	testCase.configFileData = "";
	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetStaticHtmlTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Get Static Html Test";
	testCase.description = "Test to check if the server returns a specific static HTML file directly";
	testCase.port = "1025";
	testCase.host = "localhost";
	// Explicitly asking for index.htm instead of just the / root
	testCase.request = "GET /index.htm HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 200 OK";
	testCase.configFileData = "";
	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetBinaryFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Get Binary File Test";
	testCase.description = "Test to check if the server can serve binary files (like images/icons) without null-byte string truncation.";
	testCase.port = "1025";
	testCase.host = "localhost";
	testCase.request = "GET /favicon.ico HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 200 OK";

	testCase.configFileData = "NOTE: If this fails, ensure your server testCaseuration has a root directory mapped to '/' and that a valid binary file named 'favicon.ico' exists there. Your file reading logic must use read() and buffer sizes, NOT std::getline or string functions that break on \\0.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetDirectoryWithTrailingSlashTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Get Directory With Trailing Slash Test";
	testCase.description = "Test to check if the server correctly handles requests for a directory with a trailing slash.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Requesting a directory instead of a file
	testCase.request = "GET /images/ HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 200 OK";

	testCase.configFileData = "NOTE: Ensure you have an 'images' directory inside your web root. For this to return 200 OK, the server must either have directory listing (autoindex) enabled for this path, OR there must be a valid default index file (like index.htm) inside the 'images' folder.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::HeadRequestTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Head Request Test";
	testCase.description = "Test to check if the server correctly handles a HEAD request by returning headers without a response body.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Sending a HEAD request instead of GET
	testCase.request = "HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 501 Not Implemented";

	testCase.configFileData = "NOTE: The server must return the exact same headers as a GET request (including Content-Length) but MUST NOT send any body content. Ensure your response logic doesn't skip writing the headers to the socket, and correctly closes/completes the request to avoid a timeout.";

	testCase.timeout = -1;

	RunTestCase(testCase);
}

void HappyPathTests::PostSimpleTextTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Post Simple Text Test";
	testCase.description = "Test to check if the server correctly handles a POST request with a plain text body.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Plain text body: "Hello, World!" = 13 bytes
	string body = "Hello, World!";
	string contentLength = to_string(body.size());

	testCase.request = "POST /upload HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "Content-Type: text/plain\r\n"
					   "Content-Length: " + contentLength + "\r\n"
					   "\r\n"
					   + body;

	testCase.expectedResponse = "HTTP/1.1 200 OK";

	testCase.configFileData = "NOTE: Ensure the server has a POST-enabled route for '/upload'. The route must accept 'text/plain' bodies and return 200 OK on success. The 'client_max_body_size' in your config must be at least 13 bytes.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::DeleteExistingFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Delete Existing File Test";
	testCase.description = "Test to check if the server correctly handles a DELETE request and removes an existing resource.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Targeting a file that is expected to exist in the uploads directory
	testCase.request = "DELETE /uploads/delete_me.txt HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse = "HTTP/1.1 204 No Content";

	testCase.configFileData = "NOTE: Before running this test, ensure a file named 'delete_me.txt' exists inside the server's '/uploads' directory. The route for '/uploads' must have the DELETE method allowed in the config. On success the server must return 204 No Content with no body.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetCssFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Get Css File Test";
	testCase.description = "Test to check if the server serves a CSS file with the correct Content-Type header.";
	testCase.port = "1025";
	testCase.host = "localhost";

	testCase.request = "GET /styles.css HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse = "HTTP/1.1 200 OK";

	testCase.configFileData = "NOTE: Ensure a file named 'styles.css' exists in the server's web root. The server should respond with 'Content-Type: text/css' in the headers. This test validates that the MIME type resolution for '.css' extensions is implemented correctly.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetImageFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Get Image File Test";
	testCase.description = "Test to check if the server serves a PNG image with the correct Content-Type header.";
	testCase.port = "1025";
	testCase.host = "localhost";

	testCase.request = "GET /images/test.png HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse = "HTTP/1.1 200 OK";

	testCase.configFileData = "NOTE: Ensure a valid PNG file named 'test.png' exists inside the server's '/images' directory. The server should respond with 'Content-Type: image/png'. File reading must use binary-safe read() calls, not string-based functions, to avoid truncation on null bytes.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetLargeHtmlTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Get Large Html Test";
	testCase.description = "Test to check if the server can correctly serve a large HTML file, validating that the response is fully buffered and sent across multiple write() calls if needed.";
	testCase.port = "1025";
	testCase.host = "localhost";

	testCase.request = "GET /large.html HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse = "HTTP/1.1 200 OK";

	testCase.configFileData = "NOTE: Ensure a file named 'large.html' exists in the server's web root. The file should be large enough (at minimum several KB, ideally >64KB) to force the server to send the response body across multiple send()/write() syscalls. This validates the server's chunked-send loop logic.";

	testCase.timeout = 5000;

	RunTestCase(testCase);
}

void HappyPathTests::PostUploadTextFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Post Upload Text File Test";
	testCase.description = "Test to check if the server correctly handles a multipart/form-data POST request uploading a plain text file.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Multipart boundary and body construction
	// Each \r\n is exactly 2 bytes — content length must match precisely
	string boundary    = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
	string contentDisp = "Content-Disposition: form-data; name=\"file\"; filename=\"upload_test.txt\"";
	string contentType = "Content-Type: text/plain";
	string fileContent = "This is a test upload file.\n";

	string body =
		"--" + boundary + "\r\n"
		+ contentDisp + "\r\n"
		+ contentType + "\r\n"
		+ "\r\n"
		+ fileContent + "\r\n"
		+ "--" + boundary + "--\r\n";

	string contentLength = to_string(body.size());

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse = "HTTP/1.1 201 Created";

	testCase.configFileData = "NOTE: The server must have a POST route for '/upload' that accepts multipart/form-data. On successful file save it should return 201 Created. Ensure 'client_max_body_size' is large enough and the upload directory is writable by the server process.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::PostUploadBinaryFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Post Upload Binary File Test";
	testCase.description = "Test to check if the server correctly handles a multipart/form-data POST request uploading a binary file (simulated PNG header bytes).";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Simulated minimal PNG-like binary payload (first 8 bytes of a valid PNG signature)
	// Using string constructor with explicit length to safely embed null bytes
	string boundary    = "----WebKitFormBoundary9aB3cD4eF5gH6iJ";
	string contentDisp = "Content-Disposition: form-data; name=\"file\"; filename=\"upload_test.png\"";
	string contentType = "Content-Type: image/png";

	// PNG magic bytes: \x89PNG\r\n\x1a\n followed by minimal padding
	string fileContent = string("\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR", 21);

	string bodyPrefix =
		"--" + boundary + "\r\n"
		+ contentDisp + "\r\n"
		+ contentType + "\r\n"
		+ "\r\n";

	string bodySuffix =
		"\r\n"
		"--" + boundary + "--\r\n";

	// Assemble with explicit sizes to preserve embedded null bytes
	string body = bodyPrefix + fileContent + bodySuffix;
	string contentLength = to_string(body.size());

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse = "HTTP/1.1 201 Created";

	testCase.configFileData = "NOTE: This test sends a binary payload containing null bytes (\\x00). The server's multipart parser MUST NOT use string functions like strstr() or std::string::find() on the raw body if it relies on null-termination. Use memmem() or size-aware search instead. The upload directory must be writable.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetAlternatePortTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "Get Alternate Port Test";
	testCase.description = "Test to check if the server is correctly listening and serving responses on a secondary configured port.";
	testCase.port = "1026";
	testCase.host = "localhost";

	testCase.request = "GET / HTTP/1.1\r\n"
					   "Host: localhost:1026\r\n"
					   "\r\n";

	testCase.expectedResponse = "HTTP/1.1 200 OK";

	testCase.configFileData = "NOTE: Ensure your server config has a second 'server' block listening on port 1026 with a valid root and index file. This test validates that the multiplexer/epoll loop correctly handles multiple listening sockets simultaneously.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Get Index Test",                     (void (ATestList::*)())&HappyPathTests::GetIndexTest) );
	_testFunctions.push_back( make_pair("Get Static Html Test",               (void (ATestList::*)())&HappyPathTests::GetStaticHtmlTest) );
	_testFunctions.push_back( make_pair("Get Binary File Test",               (void (ATestList::*)())&HappyPathTests::GetBinaryFileTest) );
	_testFunctions.push_back( make_pair("Get Directory With Trailing Slash Test", (void (ATestList::*)())&HappyPathTests::GetDirectoryWithTrailingSlashTest) );
	_testFunctions.push_back( make_pair("Head Request Test",                  (void (ATestList::*)())&HappyPathTests::HeadRequestTest) );
	_testFunctions.push_back( make_pair("Post Simple Text Test",              (void (ATestList::*)())&HappyPathTests::PostSimpleTextTest) );
	_testFunctions.push_back( make_pair("Delete Existing File Test",          (void (ATestList::*)())&HappyPathTests::DeleteExistingFileTest) );
	_testFunctions.push_back( make_pair("Get Css File Test",                  (void (ATestList::*)())&HappyPathTests::GetCssFileTest) );
	_testFunctions.push_back( make_pair("Get Image File Test",                (void (ATestList::*)())&HappyPathTests::GetImageFileTest) );
	_testFunctions.push_back( make_pair("Get Large Html Test",                (void (ATestList::*)())&HappyPathTests::GetLargeHtmlTest) );
	_testFunctions.push_back( make_pair("Post Upload Text File Test",         (void (ATestList::*)())&HappyPathTests::PostUploadTextFileTest) );
	_testFunctions.push_back( make_pair("Post Upload Binary File Test",       (void (ATestList::*)())&HappyPathTests::PostUploadBinaryFileTest) );
	_testFunctions.push_back( make_pair("Get Alternate Port Test",            (void (ATestList::*)())&HappyPathTests::GetAlternatePortTest) );
}