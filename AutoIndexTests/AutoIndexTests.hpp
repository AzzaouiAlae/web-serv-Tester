#pragma once
#include "../ATestList/ATestList.hpp"

class AutoIndexTests : public ATestList
{
	void AutoIndexEnabledTest();
	void AutoIndexDisabledTest();
	void AutoIndexShowsFilesTest();
	void AutoIndexContentTypeTest();
	void AutoIndexWithIndexFileTest();
	void AddAllTests();
public:
	AutoIndexTests();
	~AutoIndexTests();
};
