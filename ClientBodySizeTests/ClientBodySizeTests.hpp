#pragma once
#include "../ATestList/ATestList.hpp"

class ClientBodySizeTests : public ATestList
{
	void PostWellUnderLimitTest();
	void PostAtExactLimitTest();
	void PostOneByteOverLimitTest();
	void PostLargeBodyRejectedTest();
	void AddAllTests();
public:
	ClientBodySizeTests();
	~ClientBodySizeTests();
};
