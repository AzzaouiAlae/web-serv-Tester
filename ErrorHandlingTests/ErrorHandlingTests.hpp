#pragma once
#include "../ATestList/ATestList.hpp"

class ErrorHandlingTests : public ATestList
{
	void GetNonExistentFileTest();
	void GetForbiddenFileTest();
	void MalformedRequestLineTest();
	void UnsupportedMethodTest();
	void HttpVersionMismatchTest();
	void MissingHostHeaderTest();
	void AddAllTests();
public:
	ErrorHandlingTests();
	~ErrorHandlingTests();
};
