#pragma once
#include "../ATestList/ATestList.hpp"

class CGITests : public ATestList
{
	void CgiGetRequestTest();
	void CgiPostWithBodyTest();
	void CgiEnvVarsTest();
	void CgiNoContentLengthTest();
	void CgiQueryStringTest();
	void CgiWorkingDirectoryTest();
	void CgiTimeoutTest();
	void AddAllTests();
public:
	CGITests();
	~CGITests();
};
