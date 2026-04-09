#pragma once
#include "../ATestList/ATestList.hpp"

class ChunkedTransferTests : public ATestList
{
	void ChunkedPostSimpleTest();
	void ChunkedPostMultiChunkTest();
	void ChunkedBodyEchoedByCgiTest();
	void ChunkedBodySizeLimitTest();
	void ChunkedEmptyBodyTest();
	void ChunkedMultipartTest();

	// New multipart + chunked interaction tests (declared here, implemented later)
	void BoundarySplitAcrossChunksTest();
	void MultiFileUploadTest();
	void MultipleFieldsTest();
	void MalformedBoundaryTest();
	void MissingClosingBoundaryTest();
	void BinaryDataTest();
	void LargeFileStreamTest();
	void SlowUploadByteByByteTest();
	void FakeBoundaryInBodySlowUploadTest();
	void PrematureEOFTest();
	void ChunkExtensionsAndTrailersTest();
	void ChunkSizeEdgeCasesTest();
	void ContentDispositionAndFilenameTest();
public:
	void AddAllTests();
	ChunkedTransferTests();
	~ChunkedTransferTests();
};
