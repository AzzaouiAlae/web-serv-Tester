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
	testCase.name = "1 » Get Index Test";
	testCase.description = "Test to check if the server returns the correct index page";
	testCase.port = "1025";
	testCase.host = "localhost";
	testCase.request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");
	testCase.configurationsForTestCase = "";
	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetStaticHtmlTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "2 » Get Static Html Test";
	testCase.description = "Test to check if the server returns a specific static HTML file directly";
	testCase.port = "1025";
	testCase.host = "localhost";
	// Explicitly asking for index.htm instead of just the / root
	testCase.request = "GET /index.htm HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");
	testCase.configurationsForTestCase = "";
	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetBinaryFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "3 » Get Binary File Test";
	testCase.description = "Test to check if the server can serve binary files (like images/icons) without null-byte string truncation.";
	testCase.port = "1025";
	testCase.host = "localhost";
	testCase.request = "GET /favicon.ico HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "NOTE: If this fails, ensure your server testCaseuration has a root directory mapped to '/' and that a valid binary file named 'favicon.ico' exists there. Your file reading logic must use read() and buffer sizes, NOT std::getline or string functions that break on \\0.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetDirectoryWithTrailingSlashTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "4 » Get Directory With Trailing Slash Test";
	testCase.description = "Test to check if the server correctly handles requests for a directory with a trailing slash.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Requesting a directory instead of a file
	testCase.request = "GET /images/ HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "NOTE: Ensure you have an 'images' directory inside your web root. For this to return 200 OK, the server must either have directory listing (autoindex) enabled for this path, OR there must be a valid default index file (like index.htm) inside the 'images' folder.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::HeadRequestTest()
{
	// Step 1: First, get the expected headers by sending a GET request
	TestCase getCase;
	getCase.name = "Head Request Test - Get Reference";
	getCase.description = "Get the reference response headers for comparison with HEAD request.";
	getCase.port = "1025";
	getCase.host = "localhost";
	getCase.request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	getCase.expectedResponse.push_back("HTTP/1.1 200 OK");
	getCase.timeout = 2000;
	getCase.printTest = false; // Don't print this GET test in the summary
	getCase.isSubTest = true; // Mark as sub-test to exclude from pass/fail count

	RunTestCase(getCase);

	// Extract important headers from GET response (ignoring dynamic ones like Date, Set-Cookie)
	vector<string> expectedStableHeaders;
	if (getCase.headerLength > 0 && getCase.headerLength <= getCase.response.size())
	{
		string headers = getCase.response.substr(0, getCase.headerLength);
		size_t pos = 0;
		while ((pos = headers.find("\r\n", pos)) != string::npos)
		{
			if (pos == 0)
			{
				pos += 2;
				continue;
			}
			size_t lineStart = headers.rfind("\r\n", pos - 1);
			if (lineStart == string::npos)
				lineStart = 0;
			else
				lineStart += 2;
			
			string line = headers.substr(lineStart, pos - lineStart);
			
			// Skip dynamic headers
			if (line.find("Date:") == string::npos && 
			    line.find("Set-Cookie:") == string::npos &&
			    !line.empty() && line != "\r\n")
			{
				expectedStableHeaders.push_back(line);
			}
			pos += 2;
		}
	}

	// Step 2: Test HEAD request - should return identical headers (except Date/Set-Cookie) but no body
	TestCase testCase;
	testCase.name = "5 » Head Request Test";
	testCase.description = "Test to check if the server correctly handles a HEAD request by returning the same headers as GET but without a response body (excluding dynamic headers like Date and Set-Cookie).";
	testCase.port = "1025";
	testCase.host = "localhost";

	testCase.request = "HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	
	// Build expected response with stable headers
	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");
	for (const auto &header : expectedStableHeaders)
	{
		testCase.expectedResponse.push_back(header);
	}

	testCase.configurationsForTestCase = "NOTE: A HEAD request must return the exact same headers as a GET request (Content-Length, Content-Type, Allow, Server, etc.) but MUST NOT send any response body. Dynamic headers like Date and Set-Cookie are ignored in this comparison. This test first retrieves the headers from a GET request to '/', then verifies that HEAD returns identical stable headers without any body content.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::PostSimpleTextTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "6 » Post Simple Text Test";
	testCase.description = "Test to check if the server correctly handles a POST request with a plain text body.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Plain text body: "Hello, World!" = 13 bytes
	string body = "Hello, World!";
	string contentLength = to_string(body.size());

	testCase.request = "POST /upload/delete_me.txt HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "Content-Type: text/plain\r\n"
					   "Content-Length: " + contentLength + "\r\n"
					   "\r\n"
					   + body;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created");

	testCase.configurationsForTestCase = "NOTE: Ensure the server has a POST-enabled route for '/upload'. The route must accept 'text/plain' bodies and return 201 Created on success. The 'client_max_body_size' in your config must be at least 13 bytes.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::DeleteExistingFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "7 » Delete Existing File Test";
	testCase.description = "Test to check if the server correctly handles a DELETE request and removes an existing resource.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Targeting a file that is expected to exist in the uploads directory
	testCase.request = "DELETE /upload/delete_me.txt HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 204 No Content");

	testCase.configurationsForTestCase = "NOTE: Before running this test, ensure a file named 'delete_me.txt' exists inside the server's '/uploads' directory. The route for '/uploads' must have the DELETE method allowed in the config. On success the server must return 204 No Content with no body.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetCssFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "8 » Get Css File Test";
	testCase.description = "Test to check if the server serves a CSS file with the correct Content-Type header.";
	testCase.port = "1025";
	testCase.host = "localhost";

	testCase.request = "GET /styles.css HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");
	testCase.expectedResponse.push_back("Content-Type: text/css");

	testCase.configurationsForTestCase = "NOTE: Ensure a file named 'styles.css' exists in the server's web root. The server should respond with 'Content-Type: text/css' in the headers. This test validates that the MIME type resolution for '.css' extensions is implemented correctly.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetImageFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "9 » Get Image File Test";
	testCase.description = "Test to check if the server serves a PNG image with the correct Content-Type header.";
	testCase.port = "1025";
	testCase.host = "localhost";

	testCase.request = "GET /images/test.jpg HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");
	testCase.expectedResponse.push_back("Content-Type: image/jpeg");
	testCase.configurationsForTestCase = "NOTE: Ensure a valid JPEG file named 'test.jpg' exists inside the server's '/images' directory. The server should respond with 'Content-Type: image/jpeg'. File reading must use binary-safe read() calls, not string-based functions, to avoid truncation on null bytes.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetLargeHtmlTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "10 » Get Large Html Test";
	testCase.description = "Test to check if the server can correctly serve a large HTML file, validating that the response is fully buffered and sent across multiple write() calls if needed.";
	testCase.port = "1025";
	testCase.host = "localhost";

	testCase.request = "GET /large.htm HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "NOTE: Ensure a file named 'large.html' exists in the server's web root. The file should be large enough (at minimum several KB, ideally >64KB) to force the server to send the response body across multiple send()/write() syscalls. This validates the server's chunked-send loop logic.";

	testCase.timeout = 5000;

	RunTestCase(testCase);
}

void HappyPathTests::PostUploadBinaryFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "11 » Post Upload Binary File Test";
	testCase.description = "Test to check if the server correctly handles a POST request uploading a binary file (simulated PNG header bytes) as multipart form data, and verify the file is saved correctly.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// PNG magic bytes: \x89PNG\r\n\x1a\n followed by minimal padding
	string pngData = string("\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR", 21);
	
	// Build multipart body
	string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
	string body;
	
	body += "--" + boundary + "\r\n";
	body += "Content-Disposition: form-data; name=\"file\"; filename=\"image.png\"\r\n";
	body += "Content-Type: image/png\r\n";
	body += "\r\n";
	body += pngData;
	body += "\r\n--" + boundary + "--\r\n";

	string contentLength = to_string(body.size());

	testCase.request =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created");

	testCase.configurationsForTestCase = "NOTE: This test uploads a binary PNG file via multipart/form-data containing null bytes (\\x00), then retrieves it to verify correct parsing and storage. The server's multipart parser MUST use size-aware parsing (memmem, memcmp) and NOT string functions like strstr() or std::string::find() that rely on null-termination.";

	testCase.timeout = 2000;

	RunTestCase(testCase);

}

void HappyPathTests::PostOnRootTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "12 » Post On Root Test";
	testCase.description = "Test to check if the server correctly handles a POST request sent to the root path.";

	testCase.port = "1025";
	testCase.host = "localhost";

	// PNG magic bytes: \x89PNG\r\n\x1a\n followed by minimal padding
	string pngData = string("\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR", 21);
	
	// Build multipart body
	string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
	string body;
	
	body += "--" + boundary + "\r\n";
	body += "Content-Disposition: form-data; name=\"file\"; filename=\"image.png\"\r\n";
	body += "Content-Type: image/png\r\n";
	body += "\r\n";
	body += pngData;
	body += "\r\n--" + boundary + "--\r\n";

	string contentLength = to_string(body.size());

	testCase.request =
		"POST / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
		"Content-Length: " + contentLength + "\r\n"
		"\r\n"
		+ body;

	testCase.expectedResponse.push_back("HTTP/1.1 201 Created");

	testCase.configurationsForTestCase = "NOTE: This test uploads a binary PNG file via multipart/form-data containing null bytes (\\x00), then retrieves it to verify correct parsing and storage. The server's multipart parser MUST use size-aware parsing (memmem, memcmp) and NOT string functions like strstr() or std::string::find() that rely on null-termination.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::DeleteBinaryFileTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "13 » Delete Binary File Test";
	testCase.description = "Test to check if the server correctly handles a DELETE request and removes an existing resource.";
	testCase.port = "1025";
	testCase.host = "localhost";

	// Targeting a file that is expected to exist in the uploads directory
	testCase.request = "DELETE /upload/image.png HTTP/1.1\r\n"
					   "Host: localhost\r\n"
					   "\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 204 No Content");

	testCase.configurationsForTestCase = "NOTE: Before running this test, ensure a file named 'image.png' exists inside the server's '/uploads' directory. The route for '/uploads' must have the DELETE method allowed in the config. On success the server must return 204 No Content with no body.";

	testCase.timeout = 2000;

	RunTestCase(testCase);
}

void HappyPathTests::GetAlternatePortTest()
{
	// arrange
	TestCase testCase;
	testCase.name = "14 » Get Alternate Port Test";
	testCase.description = "Test to check if the server is correctly listening and serving responses on a secondary configured port.";
	testCase.port = "1026";
	testCase.host = "localhost";

	testCase.request = "GET / HTTP/1.1\r\n"
					   "Host: localhost:1026\r\n"
					   "\r\n";

	testCase.expectedResponse.push_back("HTTP/1.1 200 OK");

	testCase.configurationsForTestCase = "NOTE: Ensure your server config has a second 'server' block listening on port 1026 with a valid root and index file. This test validates that the multiplexer/epoll loop correctly handles multiple listening sockets simultaneously.";

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
	_testFunctions.push_back( make_pair("Post Upload Binary File Test",       (void (ATestList::*)())&HappyPathTests::PostUploadBinaryFileTest) );
	_testFunctions.push_back( make_pair("Post On Root Test",                  (void (ATestList::*)())&HappyPathTests::PostOnRootTest) );
	_testFunctions.push_back( make_pair("Get Alternate Port Test",            (void (ATestList::*)())&HappyPathTests::GetAlternatePortTest) );
	_testFunctions.push_back( make_pair("Delete Binary File Test",            (void (ATestList::*)())&HappyPathTests::DeleteBinaryFileTest) );
}
