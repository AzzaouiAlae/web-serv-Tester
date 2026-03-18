#pragma once
#include "../ATestList/ATestList.hpp"

class MinimumEvaluationTests : public ATestList
{
    
    // Basic GET/POST method restrictions tests
    void SimpleGet();
    void PostRootReturn405();
    void HeadRootReturn405();
    
    // Directory routing and redirection tests
    void DirectoryRedirectTo301();
    void DirectoryGetBadExtension();
    void DirectoryGetFileBadExtension();
    void DirectoryGetFileBlaBla();
    void DirectoryGetNonExistent();
    
    // Subdirectory navigation tests
    void NopDirectoryReturn301();
    void NopDirectoryWithReferer();
    void NopDirectoryAutoindex();
    void NopDirectoryGetFile();
    void NopDirectoryGetOtherFile();
    
    // Different root location tests
    void YeahDirectoryReturn404();
    void YeahNotHappyReturn200();
    
    // Chunked transfer encoding tests
    void ChunkedLarge100MYChars();
    void ChunkedLarge100MEChars();
    void Chunked100KEChars();
    
    // Client body size limit tests
    void PostBodyEmpty();
    void PostBody100PChars();
    void PostBody200WChars();
    void PostBody101QCharsExceedsLimit();
    
    // Client body size limit tests with chunked transfer encoding
    void ChunkedPostBodyEmpty();
    void ChunkedPostBody100PChars();
    void ChunkedPostBody200WChars();
    void ChunkedPostBody101QCharsExceedsLimit();
    
    // Stress tests
    void StressFork5Requests20();
    void StressFork20Requests5000();
    void StressFork128Requests50();
    void StressLargePOSTFork20();
    void ChunkedForkLarge100MKChars();
    
    void AddAllTests();
public:
    MinimumEvaluationTests();
    ~MinimumEvaluationTests();
};