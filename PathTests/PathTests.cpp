#include "PathTests.hpp"
#include "../Colors/Colors.hpp"

PathTests::PathTests() : ATestList("Path Tests")
{
	AddAllTests();
}

PathTests::~PathTests()
{
}

// ─────────────────────────────────────────────
//  404 - NOT FOUND
// ─────────────────────────────────────────────

void PathTests::NotFoundFileTest()
{
	TestCase testCase;
	testCase.name             = "Not Found File Test";
	testCase.description      = "Server must return 404 when requesting a file that does not exist.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET /nonexistent_file.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 404 Not Found";
	testCase.configFileData   = "NOTE: Ensure no file named 'nonexistent_file.html' exists under your web root.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

void PathTests::NotFoundDirectoryTest()
{
	TestCase testCase;
	testCase.name             = "Not Found Directory Test";
	testCase.description      = "Server must return 404 when requesting a directory path that does not exist.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET /ghost/missing/dir/ HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 404 Not Found";
	testCase.configFileData   = "NOTE: Ensure the path '/ghost/missing/dir/' does not exist under your web root.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────
//  403 - FORBIDDEN
// ─────────────────────────────────────────────

void PathTests::ForbiddenFileTest()
{
	TestCase testCase;
	testCase.name             = "Forbidden File Test";
	testCase.description      = "Server must return 403 when requesting a file with chmod 000.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET /forbidden.txt HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 403 Forbidden";
	testCase.configFileData   = "SETUP: touch <webroot>/forbidden.txt && chmod 000 <webroot>/forbidden.txt\n"
	                            "Server must NOT run as root (root bypasses permission checks).";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

void PathTests::ForbiddenDirectoryTest()
{
	TestCase testCase;
	testCase.name             = "Forbidden Directory Test";
	testCase.description      = "Server must return 403 when requesting a directory with chmod 000.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET /forbidden_dir/ HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 403 Forbidden";
	testCase.configFileData   = "SETUP: mkdir <webroot>/forbidden_dir && chmod 000 <webroot>/forbidden_dir\n"
	                            "Server must NOT run as root.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

void PathTests::DirectoryWithoutIndexTest()
{
	TestCase testCase;
	testCase.name             = "Directory Without Index Test";
	testCase.description      = "Server must return 403 for a readable directory with no index file and autoindex disabled.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET /empty_dir/ HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 403 Forbidden";
	testCase.configFileData   = "SETUP: mkdir <webroot>/empty_dir\n"
	                            "Disable autoindex for this path. Do NOT place any index file inside.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────
//  400 - MALFORMED / DANGEROUS PATHS
// ─────────────────────────────────────────────

void PathTests::PathTraversalTest()
{
	TestCase testCase;
	testCase.name             = "Path Traversal Test";
	testCase.description      = "Server must delete raw '../' and return the normalized path in the URI (security critical).";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET /../../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 404 Not Found";
	testCase.configFileData   = "WARNING: A 200 OK here is a critical security vulnerability. "
	                            "Acceptable: 404 Not Found or 400 Bad Request or 403 Forbidden.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

void PathTests::EncodedTraversalTest()
{
	TestCase testCase;
	testCase.name             = "Encoded Path Traversal Test";
	testCase.description      = "Server must reject percent-encoded traversal (%2e%2e%2f) after URI decoding.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	// %2e = '.'  %2f = '/'  ->  decodes to ../../etc/passwd
	testCase.request          = "GET /%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 404 Not Found";
	testCase.configFileData   = "NOTE: Decode percent-encoding BEFORE resolving the path. "
	                            "Then apply the same traversal check as for raw '../'.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

void PathTests::NullByteInPathTest()
{
	TestCase testCase;
	testCase.name             = "Null Byte In Path Test";
	testCase.description      = "Server must reject URIs containing %00 (null byte) – classic C-string truncation attack.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET /index%00.htm HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 400 Bad Request";
	testCase.configFileData   = "NOTE: A null byte in the URI is always invalid per RFC 3986. "
	                            "Passing %00 to open()/stat() is a security vulnerability.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

void PathTests::DoubleSlashPathTest()
{
	TestCase testCase;
	testCase.name             = "Double Slash Path Test";
	testCase.description      = "Server must normalize '//' to '/' and still serve the resource correctly.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET //index.htm HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 200 OK";
	testCase.configFileData   = "NOTE: RFC 3986 allows collapsing '//' to '/'. "
	                            "A 301 redirect to the normalised path is also acceptable.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

// ─────────────────────────────────────────────
//  414 - URI TOO LONG
// ─────────────────────────────────────────────

void PathTests::VeryLongPathTest()
{
	TestCase testCase;
	testCase.name             = "Very Long Path Test";
	testCase.description      = "Server must return 414 URI Too Long when the request URI exceeds the configured limit.";
	testCase.port             = "1025";
	testCase.host             = "localhost";
	testCase.request          = "GET /" + string(9000, 'a') + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
	testCase.expectedResponse = "HTTP/1.1 414";
	testCase.configFileData   = "NOTE: Adjust the path length to be just above your server's configured URI limit.";
	testCase.timeout          = 2000;

	RunTestCase(testCase);
}

void PathTests::AddAllTests()
{
	_testFunctions.push_back( make_pair("Get Forbidden File Test",    (void (ATestList::*)())&PathTests::ForbiddenFileTest) );
	_testFunctions.push_back( make_pair(" Forbidden Directory Test",    (void (ATestList::*)())&PathTests::ForbiddenDirectoryTest) );
	_testFunctions.push_back( make_pair("Directory Without Index Test",    (void (ATestList::*)())&PathTests::DirectoryWithoutIndexTest) );
	_testFunctions.push_back( make_pair("Path Traversal Test",    (void (ATestList::*)())&PathTests::PathTraversalTest) );
	_testFunctions.push_back( make_pair("Encoded Path Traversal Test",    (void (ATestList::*)())&PathTests::EncodedTraversalTest) );
	_testFunctions.push_back( make_pair("Null Byte In Path Test",    (void (ATestList::*)())&PathTests::NullByteInPathTest) );
	_testFunctions.push_back( make_pair("Double Slash Path Test",    (void (ATestList::*)())&PathTests::DoubleSlashPathTest) );
	_testFunctions.push_back( make_pair("Very Long Path Test",    (void (ATestList::*)())&PathTests::VeryLongPathTest) );
	_testFunctions.push_back( make_pair("Not Found File Test",    (void (ATestList::*)())&PathTests::NotFoundFileTest) );
	_testFunctions.push_back( make_pair("Not Found Directory Test",    (void (ATestList::*)())&PathTests::NotFoundDirectoryTest) );
}

// // ─────────────────────────────────────────────
// //  RUNNER
// // ─────────────────────────────────────────────

// void PathTests::RunAllTests()
// {
// 	NotFoundFileTest();
// 	NotFoundDirectoryTest();
// 	ForbiddenFileTest();
// 	ForbiddenDirectoryTest();
// 	DirectoryWithoutIndexTest();
// 	PathTraversalTest();
// 	EncodedTraversalTest();
// 	NullByteInPathTest();
// 	DoubleSlashPathTest();
// 	VeryLongPathTest();
// }

// void PathTests::performTestCase(int choice)
// {
// 	switch (choice)
// 	{
// 	case 0:  RunAllTests();               break;
// 	case 1:  NotFoundFileTest();          break;
// 	case 2:  NotFoundDirectoryTest();     break;
// 	case 3:  ForbiddenFileTest();         break;
// 	case 4:  ForbiddenDirectoryTest();    break;
// 	case 5:  DirectoryWithoutIndexTest(); break;
// 	case 6:  PathTraversalTest();         break;
// 	case 7:  EncodedTraversalTest();      break;
// 	case 8:  NullByteInPathTest();        break;
// 	case 9:  DoubleSlashPathTest();       break;
// 	case 10: VeryLongPathTest();          break;
// 	case 11:
// 		cout << CLR_WARN "  Returning to main menu..." RESET << endl;
// 		break;
// 	default:
// 		cout << CLR_ERROR "  Invalid choice. Please try again." RESET << endl;
// 		break;
// 	}
// }

// // ─────────────────────────────────────────────
// //  ShowTestsList  –  colourised menu
// // ─────────────────────────────────────────────

// void PathTests::ShowTestsList()
// {
// 	int choice;
// 	do
// 	{
// 		// ── Header ──────────────────────────────────────────────────────
// 		cout << CLR_TITLE
// 		     << "╔══════════════════════════════════════════╗\n"
// 		     << "║           PATH TESTS  SUITE              ║\n"
// 		     << "╚══════════════════════════════════════════╝"
// 		     << RESET << "\n\n";

// 		// ── Run all ─────────────────────────────────────────────────────
// 		cout << CLR_MENU_NUM "  0." CLR_MENU_OPT "  Run All Tests" RESET << "\n\n";

// 		// ── 404 ─────────────────────────────────────────────────────────
// 		cout << CLR_MENU_CAT "  ── 404 Not Found ──────────────────────" RESET << "\n";
// 		cout << CLR_MENU_NUM "  1." CLR_MENU_OPT "  NotFoundFileTest" RESET << "\n";
// 		cout << CLR_MENU_NUM "  2." CLR_MENU_OPT "  NotFoundDirectoryTest" RESET << "\n\n";

// 		// ── 403 ─────────────────────────────────────────────────────────
// 		cout << CLR_MENU_CAT "  ── 403 Forbidden ──────────────────────" RESET << "\n";
// 		cout << CLR_MENU_NUM "  3." CLR_MENU_OPT "  ForbiddenFileTest" RESET << "\n";
// 		cout << CLR_MENU_NUM "  4." CLR_MENU_OPT "  ForbiddenDirectoryTest" RESET << "\n";
// 		cout << CLR_MENU_NUM "  5." CLR_MENU_OPT "  DirectoryWithoutIndexTest" RESET << "\n\n";

// 		// ── 400 ─────────────────────────────────────────────────────────
// 		cout << CLR_MENU_CAT "  ── 400 Bad Request ────────────────────" RESET << "\n";
// 		cout << CLR_MENU_NUM "  6." CLR_MENU_OPT "  PathTraversalTest" RESET << "\n";
// 		cout << CLR_MENU_NUM "  7." CLR_MENU_OPT "  EncodedTraversalTest" RESET << "\n";
// 		cout << CLR_MENU_NUM "  8." CLR_MENU_OPT "  NullByteInPathTest" RESET << "\n";
// 		cout << CLR_MENU_NUM "  9." CLR_MENU_OPT "  DoubleSlashPathTest" RESET << "\n\n";

// 		// ── 414 ─────────────────────────────────────────────────────────
// 		cout << CLR_MENU_CAT "  ── 414 URI Too Long ───────────────────" RESET << "\n";
// 		cout << CLR_MENU_NUM " 10." CLR_MENU_OPT "  VeryLongPathTest" RESET << "\n\n";

// 		// ── Return ──────────────────────────────────────────────────────
// 		cout << CLR_MENU_NUM " 11." CLR_WARN "  Return" RESET << "\n";
// 		cout << CLR_DIVIDER "  ────────────────────────────────────────" RESET << "\n";

// 		cout << CLR_PROMPT "\n  Enter your choice: " RESET;
// 		cin >> choice;
// 		system("clear");
// 		performTestCase(choice);
// 	} while (choice != 11);
// }