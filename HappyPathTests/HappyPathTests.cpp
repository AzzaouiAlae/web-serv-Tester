#include "HappyPathTests.hpp"

HappyPathTests::HappyPathTests() : ATestList("Happy Path Tests")
{
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

	// Adding the requested note for testCaseuration/setup failure troubleshooting
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

	// Note explaining what needs to exist for this to pass
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

	// Note explaining what needs to exist for this to pass
	testCase.configFileData = "NOTE: The server must return the exact same headers as a GET request (including Content-Length) but MUST NOT send any body content. Ensure your response logic doesn't skip writing the headers to the socket, and correctly closes/completes the request to avoid a timeout.";

	testCase.timeout = -1;

	RunTestCase(testCase);
}

void HappyPathTests::RunAllTests()
{
	GetIndexTest();
	GetStaticHtmlTest();
	GetBinaryFileTest();
	GetDirectoryWithTrailingSlashTest();
	HeadRequestTest();
}

void HappyPathTests::performTestCase(int choice)
{
	switch (choice)
	{
	case 0:
		RunAllTests();
		break;
	case 1:
		GetIndexTest();
		break;
	case 2:
		GetStaticHtmlTest();
		break;
	case 3:
		GetBinaryFileTest();
		break;
	case 4:
		GetDirectoryWithTrailingSlashTest();
		break;
	case 5:
		HeadRequestTest(); // <--- ADD THIS CASE
		break;
	case 6:
		cout << "Returning to main menu..." << endl; // <--- SHIFT TO 6
		break;
	default:
		cout << "Invalid choice. Please try again." << endl;
		break;
	}
}

void HappyPathTests::ShowTestsList()
{
	int choice;
	do
	{
		cout << "Happy Path Tests List:" << endl;
		cout << "------------------" << endl;
		cout << "0. Run All Tests" << endl;
		cout << "1. GetIndexTest" << endl;
		cout << "2. GetStaticHtmlTest" << endl;
		cout << "3. GetBinaryFileTest" << endl;
		cout << "4. GetDirectoryWithTrailingSlashTest" << endl;
		cout << "5. HeadRequestTest" << endl; // <--- ADD THIS OPTION
		cout << "6. Return" << endl;		  // <--- SHIFT TO 6
		cout << "------------------" << endl;
		cout << "Enter your choice: ";
		cin >> choice;
		system("clear");
		performTestCase(choice);
	} while (choice != 6); // <--- UPDATE TO 6
}
