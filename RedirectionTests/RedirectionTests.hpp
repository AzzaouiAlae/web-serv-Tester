#pragma once
#include "../ATestList/ATestList.hpp"

class RedirectionTests : public ATestList
{
	void PermanentRedirectStatusTest();
	void PermanentRedirectLocationHeaderTest();
	void TemporaryRedirectStatusTest();
	void TemporaryRedirectLocationHeaderTest();
	void RedirectToExternalUrlTest();
	void AddAllTests();
public:
	RedirectionTests();
	~RedirectionTests();
};
