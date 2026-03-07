#pragma once
#include "../ATestList/ATestList.hpp"

class ChunkedTransferTests : public ATestList
{
	void ChunkedPostSimpleTest();
	void ChunkedPostMultiChunkTest();
	void ChunkedBodyEchoedByCgiTest();
	void ChunkedBodySizeLimitTest();
	void ChunkedEmptyBodyTest();
	void AddAllTests();
public:
	ChunkedTransferTests();
	~ChunkedTransferTests();
};
