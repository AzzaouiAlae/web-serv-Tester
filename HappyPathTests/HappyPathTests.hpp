#pragma once
#include "../ATestList/ATestList.hpp"


class HappyPathTests: public ATestList
{
	void GetIndexTest();
	void GetStaticHtmlTest();
	void RunAllTests();
	void GetBinaryFileTest();
	void GetDirectoryWithTrailingSlashTest();
	void HeadRequestTest();
	void performTestCase(int choice);
public:
	HappyPathTests();
	~HappyPathTests();
	void ShowTestsList();
};
