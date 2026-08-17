#include "MinimumEvaluationTests.hpp"
#include <sstream>

MinimumEvaluationTests::MinimumEvaluationTests() : ATestList("Minimum Evaluation Tests")
{
    AddAllTests();
}

MinimumEvaluationTests::~MinimumEvaluationTests()
{
}

void MinimumEvaluationTests::AddAllTests()
{
    // Tests 1-3: Basic GET/POST method restrictions tests
    _testFunctions.push_back(make_pair("Simple GET", (void (ATestList::*)())&MinimumEvaluationTests::SimpleGet));
    _testFunctions.push_back(make_pair("POST to root (405)", (void (ATestList::*)())&MinimumEvaluationTests::PostRootReturn405));
    _testFunctions.push_back(make_pair("HEAD to root (405)", (void (ATestList::*)())&MinimumEvaluationTests::HeadRootReturn405));

    // Tests 4-8: Directory routing and redirection tests
    _testFunctions.push_back(make_pair("Directory redirect (301)", (void (ATestList::*)())&MinimumEvaluationTests::DirectoryRedirectTo301));
    _testFunctions.push_back(make_pair("Directory GET index youpi.bad_extension", (void (ATestList::*)())&MinimumEvaluationTests::DirectoryGetBadExtension));
    _testFunctions.push_back(make_pair("GET /directory/youpi.bad_extension", (void (ATestList::*)())&MinimumEvaluationTests::DirectoryGetFileBadExtension));
    _testFunctions.push_back(make_pair("GET /directory/youpi.bla (no CGI)", (void (ATestList::*)())&MinimumEvaluationTests::DirectoryGetFileBlaBla));
    _testFunctions.push_back(make_pair("GET /directory/oulalala (404)", (void (ATestList::*)())&MinimumEvaluationTests::DirectoryGetNonExistent));

    // Tests 9-13: Subdirectory navigation tests
    _testFunctions.push_back(make_pair("GET /directory/nop (301)", (void (ATestList::*)())&MinimumEvaluationTests::NopDirectoryReturn301));
    _testFunctions.push_back(make_pair("GET /directory/nop/ with referer", (void (ATestList::*)())&MinimumEvaluationTests::NopDirectoryWithReferer));
    _testFunctions.push_back(make_pair("GET /directory/nop/ autoindex", (void (ATestList::*)())&MinimumEvaluationTests::NopDirectoryAutoindex));
    _testFunctions.push_back(make_pair("GET /directory/nop/other.pouic", (void (ATestList::*)())&MinimumEvaluationTests::NopDirectoryGetFile));
    _testFunctions.push_back(make_pair("GET /directory/nop/other.pouac (404)", (void (ATestList::*)())&MinimumEvaluationTests::NopDirectoryGetOtherFile));

    // Tests 14-15: Different root location tests
    _testFunctions.push_back(make_pair("GET /directory/Yeah (404)", (void (ATestList::*)())&MinimumEvaluationTests::YeahDirectoryReturn404));
    _testFunctions.push_back(make_pair("GET /directory/Yeah/not_happy.bad_extension", (void (ATestList::*)())&MinimumEvaluationTests::YeahNotHappyReturn200));

    // Tests 16-19: Large body streaming tests (chunked + Content-Length)
    _testFunctions.push_back(make_pair("POST 100M 'y' chars chunked", (void (ATestList::*)())&MinimumEvaluationTests::ChunkedLarge100MYChars));
    _testFunctions.push_back(make_pair("POST 100M 'y' chars Content-Length", (void (ATestList::*)())&MinimumEvaluationTests::ContentLengthLarge100MYChars));
    _testFunctions.push_back(make_pair("POST 100M 'e' chars chunked", (void (ATestList::*)())&MinimumEvaluationTests::ChunkedLarge100MEChars));
    _testFunctions.push_back(make_pair("POST 100K 'e' chars chunked with header", (void (ATestList::*)())&MinimumEvaluationTests::Chunked100KEChars));

    // Tests 19-22: Client body size limit tests with chunked transfer encoding
    _testFunctions.push_back(make_pair("Chunked POST /post_body empty", (void (ATestList::*)())&MinimumEvaluationTests::ChunkedPostBodyEmpty));
    _testFunctions.push_back(make_pair("Chunked POST /post_body 100 bytes (at limit)", (void (ATestList::*)())&MinimumEvaluationTests::ChunkedPostBody100PChars));
    _testFunctions.push_back(make_pair("Chunked POST /post_body 200 bytes (413)", (void (ATestList::*)())&MinimumEvaluationTests::ChunkedPostBody200WChars));
    _testFunctions.push_back(make_pair("Chunked POST /post_body 101 bytes (413)", (void (ATestList::*)())&MinimumEvaluationTests::ChunkedPostBody101QCharsExceedsLimit));

    // Tests 23-26: Stress tests (forked)
    _testFunctions.push_back(make_pair("Stress fork 5 requests 20", (void (ATestList::*)())&MinimumEvaluationTests::StressFork5Requests20));
    _testFunctions.push_back(make_pair("Stress fork 20 requests 5000", (void (ATestList::*)())&MinimumEvaluationTests::StressFork20Requests5000));
    _testFunctions.push_back(make_pair("Stress fork 128 requests 50", (void (ATestList::*)())&MinimumEvaluationTests::StressFork128Requests50));
    _testFunctions.push_back(make_pair("Chunked fork 100M 'k' chars (fork=20, req=5)", (void (ATestList::*)())&MinimumEvaluationTests::ChunkedForkLarge100MKChars));

    // Extra tests for extended coverage
    _testFunctions.push_back(make_pair("Stress large POST fork 20", (void (ATestList::*)())&MinimumEvaluationTests::StressLargePOSTFork20));
    _testFunctions.push_back(make_pair("POST /post_body empty (Content-Length)", (void (ATestList::*)())&MinimumEvaluationTests::PostBodyEmpty));
    _testFunctions.push_back(make_pair("POST /post_body 100 bytes (Content-Length)", (void (ATestList::*)())&MinimumEvaluationTests::PostBody100PChars));
    _testFunctions.push_back(make_pair("POST /post_body 200 bytes (413)", (void (ATestList::*)())&MinimumEvaluationTests::PostBody200WChars));
    _testFunctions.push_back(make_pair("POST /post_body 101 bytes (413)", (void (ATestList::*)())&MinimumEvaluationTests::PostBody101QCharsExceedsLimit));
}
// ========================= Basic GET/POST method restrictions tests =========================

void MinimumEvaluationTests::SimpleGet()
{
    TestCase config;
    config.name = "1 » Simple GET";
    config.description = "Test simple GET request to root - should return 200";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET / HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "The server should response on GET only / with 200 OK status code.";
    RunTestCase(config);
}

void MinimumEvaluationTests::PostRootReturn405()
{
    TestCase config;
    config.name = "2 » POST to root returns 405";
    config.description = "Test POST request to root - should return 405 Method Not Allowed";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "POST / HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: test/file\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n"
                     "0\r\n"
                     "\r\n";
    config.expectedResponse.push_back("405 Method Not Allowed");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "The server should response on / any other method then GET  with 405 Method Not Allowed status code.";
    RunTestCase(config);
}

void MinimumEvaluationTests::HeadRootReturn405()
{
    TestCase config;
    config.name = "3 » HEAD to root returns 405";
    config.description = "Test HEAD request to root - should return 405 Method Not Allowed";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "HEAD / HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "\r\n";
    config.expectedResponse.push_back("405 Method Not Allowed");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "The server should response on / any other method then HEAD with 405 Method Not Allowed status code.";
    RunTestCase(config);
}

// ========================= Directory routing and redirection tests =========================

void MinimumEvaluationTests::DirectoryRedirectTo301()
{
    TestCase config;
    config.name = "4 » Directory redirect returns 301";
    config.description = "Test GET request to /directory without trailing slash - should redirect to /directory/";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("301 Moved Permanently");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory without trailing slash should return 301 Moved Permanently redirect to /directory/.";
    RunTestCase(config);
}

void MinimumEvaluationTests::DirectoryGetBadExtension()
{
    TestCase config;
    config.name = "5 » Directory GET with bad extension index";
    config.description = "Test GET request to /directory/ - should return index youpi.bad_extension";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/ HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Referer: http://127.0.0.1:1027/directory\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/ should use index youpi.bad_extension and return 200 OK with the file content.";
    RunTestCase(config);
}

void MinimumEvaluationTests::DirectoryGetFileBadExtension()
{
    TestCase config;
    config.name = "6 » Directory GET file with bad extension";
    config.description = "Test GET request to /directory/youpi.bad_extension - should return 200";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/youpi.bad_extension HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/youpi.bad_extension should return 200 OK with the file content.";
    RunTestCase(config);
}

void MinimumEvaluationTests::DirectoryGetFileBlaBla()
{
    TestCase config;
    config.name = "7 » Directory GET file with bla extension";
    config.description = "Test GET request to /directory/youpi.bla - should not execute CGI and return 200";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/youpi.bla HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/youpi.bla should not trigger CGI execution because it's not POST, return 200 OK.";
    RunTestCase(config);
}

void MinimumEvaluationTests::DirectoryGetNonExistent()
{
    TestCase config;
    config.name = "8 » Directory GET non-existent file";
    config.description = "Test GET request to /directory/oulalala - should return 404";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/oulalala HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("404 Not Found");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/oulalala with non-existent file should return 404 Not Found.";
    RunTestCase(config);
}

// ========================= Subdirectory navigation tests =========================

void MinimumEvaluationTests::NopDirectoryReturn301()
{
    TestCase config;
    config.name = "9 » Nop directory returns 301";
    config.description = "Test GET request to /directory/nop without trailing slash - should redirect to /directory/nop/";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/nop HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("301 Moved Permanently");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/nop without trailing slash should return 301 Moved Permanently redirect to /directory/nop/.";
    RunTestCase(config);
}

void MinimumEvaluationTests::NopDirectoryWithReferer()
{
    TestCase config;
    config.name = "10 » Nop directory with referer returns autoindex";
    config.description = "Test GET request to /directory/nop/ with referer header - should return 200 with autoindex";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/nop/ HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Referer: http://127.0.0.1:1027/directory/nop\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/nop/ with referer should return 200 OK with autoindex showing files in the directory.";
    RunTestCase(config);
}

void MinimumEvaluationTests::NopDirectoryAutoindex()
{
    TestCase config;
    config.name = "11 » Nop directory autoindex enabled returns 200";
    config.description = "Test GET request to /directory/nop/ without referer - should return 200 with autoindex";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/nop/ HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/nop/ should return 200 OK with autoindex showing files in the directory.";
    RunTestCase(config);
}

void MinimumEvaluationTests::NopDirectoryGetFile()
{
    TestCase config;
    config.name = "12 » Nop directory GET file returns 200";
    config.description = "Test GET request to /directory/nop/other.pouic - should return 200";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/nop/other.pouic HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/nop/other.pouic should return 200 OK with the file content.";
    RunTestCase(config);
}

void MinimumEvaluationTests::NopDirectoryGetOtherFile()
{
    TestCase config;
    config.name = "13 » Nop directory GET other file returns 404";
    config.description = "Test GET request to /directory/nop/other.pouac - should return 404";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/nop/other.pouac HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("404 Not Found");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/nop/other.pouac should return 404 Not Found.";
    RunTestCase(config);
}

// ========================= Different root location tests =========================

void MinimumEvaluationTests::YeahDirectoryReturn404()
{
    TestCase config;
    config.name = "14 » Yeah directory returns 404";
    config.description = "Test GET request to /directory/Yeah without file - should return 404 due to different roots";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/Yeah HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("404 Not Found");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/Yeah should return 404 because location has different root than /directory/ or you can do it with an no existing index.";
    RunTestCase(config);
}

void MinimumEvaluationTests::YeahNotHappyReturn200()
{
    TestCase config;
    config.name = "15 » Yeah not happy returns 200";
    config.description = "Test GET request to /directory/Yeah/not_happy.bad_extension - should return 200";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "GET /directory/Yeah/not_happy.bad_extension HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "GET /directory/Yeah/not_happy.bad_extension should return 200 OK (specific location has its own root).";
    RunTestCase(config);
}

// ========================= Chunked transfer encoding tests =========================

void MinimumEvaluationTests::ChunkedLarge100MYChars()
{
    TestCase config;
    config.name = "16 » Chunked large 100M chars body with y";
    config.description = "Test POST request with chunked transfer encoding sending 100M 'y' characters";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "POST /directory/youpi.bla HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: test/file\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.body = "'y' repeated 100000000 times chunked size 32768";
    config.expectedResponse.push_back("200 OK||201 Created");
    config.expectedResponse.push_back("'Y' repeated 100000000 times");
    config.timeout = 60000;
    config.sleepTime = 10000;
    config.maxSend = 32768;
    config.configurationsForTestCase = "POST /directory/youpi.bla with 100M 'y' chars in chunked transfer should return 200 OK or 201 Created.\nBody generated on-the-fly to minimize memory usage.";
    RunStreamingTestCase(config);
}

void MinimumEvaluationTests::ContentLengthLarge100MYChars()
{
    TestCase config;
    config.name = "16b » Content-Length large 100M chars body with y";
    config.description = "Test POST request with Content-Length sending 100M 'y' characters without chunked transfer";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "POST /directory/youpi.bla HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Content-Length: 100000000\r\n"
                     "content-type: test/file\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.body = "'y' repeated 100000000 times";
    config.expectedResponse.push_back("200 OK||201 Created");
    config.expectedResponse.push_back("'Y' repeated 100000000 times");
    config.timeout = 60000;
    config.sleepTime = 10000;
    config.maxSend = 32768;
    config.configurationsForTestCase = "POST /directory/youpi.bla with 100M 'y' chars and Content-Length should return 200 OK or 201 Created.\nBody is generated progressively in small segments to avoid allocating a 100M buffer.";
    RunStreamingTestCase(config);
}

void MinimumEvaluationTests::ChunkedLarge100MEChars()
{
    TestCase config;
    config.name = "17 » Chunked large 100M chars body with e";
    config.description = "Test POST request with chunked transfer encoding sending 100M 'e' characters";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "POST /directory/youpla.bla HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: test/file\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.body = "'e' repeated 100000000 times chunked size 32768";
    config.expectedResponse.push_back("200 OK||201 Created");
    config.expectedResponse.push_back("'E' repeated 100000000 times");
    config.timeout = 60000;
    config.sleepTime = 10000;
    config.maxSend = 32768;
    config.configurationsForTestCase = "POST /directory/youpla.bla with 100M 'e' chars in chunked transfer should return 200 OK or 201 Created.\nBody generated on-the-fly to minimize memory usage.";
    RunStreamingTestCase(config);
}

void MinimumEvaluationTests::Chunked100KEChars()
{
    TestCase config;
    config.name = "18 » Chunked 100K chars body";
    config.description = "Test POST request with chunked transfer encoding sending 100K 'e' characters";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "POST /directory/youpi.bla HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: test/file\r\n"
                     "X-Secret-Header-For-Test: 1\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.body = "'e' repeated 100000 times chunked size 32768";
    config.expectedResponse.push_back("200 OK||201 Created");
    config.expectedResponse.push_back("'1' repeated 100000 times");
    config.timeout = 60000;
    config.sleepTime = 10000;
    config.maxSend = 32768;
    config.configurationsForTestCase = "POST /directory/youpi.bla with 100K 'e' chars in chunked transfer should return 200 OK or 201 Created.\nBody generated on-the-fly to minimize memory usage.";
    RunStreamingTestCase(config);
}

// ========================= Client body size limit tests =========================

void MinimumEvaluationTests::PostBodyEmpty()
{
    TestCase config;
    config.name = "19 » POST /post_body with empty body";
    config.description = "Test POST request to /post_body with empty body - should return 200 (within 100-byte limit)";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Content-Length: 0\r\n"
                     "content-type: text/plain\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK||201 Created");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "POST /post_body with empty body (0 bytes) should return 200 OK or 201 Created (within 100-byte limit).";
    RunTestCase(config);
}

void MinimumEvaluationTests::PostBody100PChars()
{
    TestCase config;
    config.name = "20 » POST /post_body with 100 bytes";
    config.description = "Test POST request to /post_body with exactly 100 bytes - should return 200 (at limit)";
    config.host = "127.0.0.1";
    config.port = "1027";

    string body(100, 'P'); // Exactly 100 'P' characters
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Content-Length: 100\r\n"
                     "content-type: text/plain\r\n"
                     "\r\n" +
                     body;
    config.expectedResponse.push_back("200 OK||201 Created");
    config.timeout = 50000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "POST /post_body with exactly 100 bytes (at limit) should return 200 OK or 201 Created.";
    RunTestCase(config);
}

void MinimumEvaluationTests::PostBody200WChars()
{
    TestCase config;
    config.name = "21 » POST /post_body with 200 bytes";
    config.description = "Test POST request to /post_body with 200 bytes - should return 413 Payload Too Large";
    config.host = "127.0.0.1";
    config.port = "1027";

    string body(200, 'W'); // 200 'W' characters (exceeds 100-byte limit)
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Content-Length: 200\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"

                     "content-type: text/plain\r\n"
                     "\r\n" +
                     body;
    config.expectedResponse.push_back("413 Payload Too Large||413 Entity Too Large");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "POST /post_body with 200 bytes (exceeds 100-byte limit) should return 413 Payload Too Large error.";
    RunTestCase(config);
}

void MinimumEvaluationTests::PostBody101QCharsExceedsLimit()
{
    TestCase config;
    config.name = "22 » POST /post_body with 101 bytes exceeds limit";
    config.description = "Test POST request to /post_body with 101 bytes - should return 413 Payload Too Large";
    config.host = "127.0.0.1";
    config.port = "1027";

    string body(101, 'Q'); // 101 'Q' characters (exceeds 100-byte limit by 1 byte)
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Content-Length: 101\r\n"
                     "content-type: text/plain\r\n"
                     "\r\n" +
                     body;
    config.expectedResponse.push_back("413 Payload Too Large||413 Entity Too Large");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "POST /post_body with 101 bytes (exceeds 100-byte limit by 1 byte) should return 413 Payload Too Large error.";
    RunTestCase(config);
}

// ========================= Client body size limit tests with chunked transfer encoding =========================

void MinimumEvaluationTests::ChunkedPostBodyEmpty()
{
    TestCase config;
    config.name = "23 » Chunked POST /post_body with empty body";
    config.description = "Test POST request to /post_body with chunked empty body - should return 200 (within 100-byte limit)";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: text/plain\r\n"
                     "\r\n"
                     "0\r\n"
                     "\r\n";
    config.expectedResponse.push_back("200 OK||201 Created");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "Chunked POST /post_body with empty body (0 bytes) should return 200 OK or 201 Created (within 100-byte limit).";
    RunTestCase(config);
}

void MinimumEvaluationTests::ChunkedPostBody100PChars()
{
    TestCase config;
    config.name = "24 » Chunked POST /post_body with 100 bytes";
    config.description = "Test POST request to /post_body with chunked body of exactly 100 bytes - should return 200 (at limit)";
    config.host = "127.0.0.1";
    config.port = "1027";

    string body(100, 'P'); // Exactly 100 'P' characters
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: text/plain\r\n"
                     "\r\n"
                     "64\r\n" +
                     body + "\r\n0\r\n\r\n"; // 0x64 = 100 in decimal
    config.expectedResponse.push_back("200 OK||201 Created");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "Chunked POST /post_body with exactly 100 bytes (at limit) should return 200 OK or 201 Created.";
    RunTestCase(config);
}

void MinimumEvaluationTests::ChunkedPostBody200WChars()
{
    TestCase config;
    config.name = "25 » Chunked POST /post_body with 200 bytes";
    config.description = "Test POST request to /post_body with chunked body of 200 bytes - should return 413 Payload Too Large";
    config.host = "127.0.0.1";
    config.port = "1027";

    string body(200, 'W'); // 200 'W' characters (exceeds 100-byte limit)
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: text/plain\r\n"
                     "\r\n"
                     "c8\r\n" +
                     body + "\r\n0\r\n\r\n"; // 0xc8 = 200 in decimal
    config.expectedResponse.push_back("413 Payload Too Large||413 Entity Too Large");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "Chunked POST /post_body with 200 bytes (exceeds 100-byte limit) should return 413 Payload Too Large error.";
    RunTestCase(config);
}

void MinimumEvaluationTests::ChunkedPostBody101QCharsExceedsLimit()
{
    TestCase config;
    config.name = "26 » Chunked POST /post_body with 101 bytes exceeds limit";
    config.description = "Test POST request to /post_body with chunked body of 101 bytes - should return 413 Payload Too Large";
    config.host = "127.0.0.1";
    config.port = "1027";

    string body(101, 'Q'); // 101 'Q' characters (exceeds 100-byte limit by 1 byte)
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: text/plain\r\n"
                     "\r\n"
                     "65\r\n" +
                     body + "\r\n0\r\n\r\n"; // 0x65 = 101 in decimal
    config.expectedResponse.push_back("413 Payload Too Large||413 Entity Too Large");
    config.timeout = 2000;
    config.sleepTime = 100000;
    config.maxSend = 10;
    config.configurationsForTestCase = "Chunked POST /post_body with 101 bytes (exceeds 100-byte limit by 1 byte) should return 413 Payload Too Large error.";
    RunTestCase(config);
}

// ========================= Stress tests =========================

void MinimumEvaluationTests::StressFork5Requests20()
{
    TestCase config;
    config.request = "GET / HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Connection: close\r\n"
                     "\r\n";
    config.name = "27 » Stress fork 5 requests 20";
    config.description = "Stress test: 5 forks × 20 requests = 100 sequential GET requests";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 2000;
    config.sleepTime = 0; // No delay between requests
    config.maxSend = 10;
    config.configurationsForTestCase = "Send 100 sequential GET requests to / to stress test the server.";
    config.printTest = false;  // Don't print each request in child processes
    config.totalRequests = 20; // 20 requests per child
    config.forkCount = 5;
    config.childResults.resize(config.forkCount, 0);

    RunForkChildTestCase(config);
}

void MinimumEvaluationTests::StressFork20Requests5000()
{
    TestCase config;
    config.request = "GET / HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "Connection: close\r\n"
                     "\r\n";
    config.name = "28 » Stress fork 20 requests 5000";
    config.description = "Stress test: 20 forks × 5000 requests = 100,000 sequential GET requests";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.expectedResponse.push_back("200 OK");
    config.timeout = 5000;
    config.sleepTime = 0;
    config.maxSend = 10;
    config.configurationsForTestCase = "Send 100,000 sequential GET requests to / to intensive stress test the server.";
    config.printTest = false;
    config.forkCount = 20;
    config.totalRequests = 5000;
    config.childResults.resize(config.forkCount, 0);

    RunForkChildTestCase(config);
}

void MinimumEvaluationTests::StressFork128Requests50()
{
    TestCase config;
    config.request = "GET /directory/nop HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "Connection: close\r\n"
                     "\r\n";
    config.name = "29 » Stress fork 128 requests 50";
    config.description = "Stress test: 128 forks × 50 requests = 6,400 GET requests to /directory/nop (redirect)";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.expectedResponse.push_back("301 Moved Permanently");
    config.timeout = 20000;
    config.sleepTime = 0;
    config.maxSend = 10;
    config.configurationsForTestCase = "Send 6,400 GET requests to /directory/nop to stress test server with redirect handling.";
    config.printTest = false;
    config.forkCount = 128;
    config.totalRequests = 50;
    config.childResults.resize(config.forkCount, 0);

    RunForkChildTestCase(config);
}

void MinimumEvaluationTests::StressLargePOSTFork20()
{
    string body(10240, 'X'); // 10KB body
    TestCase config;
    config.request = "POST /post_body HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "Content-Length: " + to_string(body.size()) + "\r\n"
                     "content-type: text/plain\r\n"
                     "Connection: close\r\n"
                     "\r\n" + body;
    config.name = "30 » Stress large POST fork 20";
    config.description = "Stress test: 20 forks × 1 large POST request with 10KB body";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.expectedResponse.push_back("413 Payload Too Large||413 Entity Too Large");
    config.timeout = 2000;
    config.sleepTime = 0;
    config.maxSend = 10;
    config.configurationsForTestCase = "Send 20 large POST requests with 10KB bodies to stress test the server.";
    config.printTest = false;
    config.printForkFailureDetails = true;
    config.forkCount = 20;
    config.totalRequests = 1;
    config.childResults.resize(config.forkCount, 0);


    RunForkChildTestCase(config);
}

void MinimumEvaluationTests::ChunkedForkLarge100MKChars()
{
    TestCase config;
    config.name = "31 » Chunked fork large 100M chars with k";
    config.description = "Stress test: 20 forks × 5 requests with 100M 'k' characters each";
    config.host = "127.0.0.1";
    config.port = "1027";
    config.request = "POST /directory/youpi.bla HTTP/1.1\r\n"
                     "Host: 127.0.0.1:1027\r\n"
                     "User-Agent: Go-http-client/1.1\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "content-type: test/file\r\n"
                     "Accept-Encoding: gzip\r\n"
                     "\r\n";
    config.body = "'k' repeated 100000000 times chunked size 32768";
    config.expectedResponse.push_back("200 OK||201 Created");
    config.expectedResponse.push_back("'K' repeated 100000000 times");
    config.timeout = 60000;
    config.sleepTime = 0;
    config.maxSend = 32768;
    config.configurationsForTestCase = "POST /directory/youpi.bla with 100M 'k' chars in chunked transfer via 20 forked processes (5 requests each) should return 200 OK or 201 Created.\nBody generated on-the-fly per child process to minimize memory usage.";
    config.printTest = false;
    config.forkCount = 20;
    config.totalRequests = 5;
    config.childResults.resize(config.forkCount, 0);
    // RunStreamingTestCase(config);
    RunForkChildTestCase(config);
}
