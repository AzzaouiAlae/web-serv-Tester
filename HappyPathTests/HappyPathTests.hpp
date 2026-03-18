#pragma once
#include "../ATestList/ATestList.hpp"


class HappyPathTests: public ATestList
{
	// Original tests
	void GetIndexTest();
	void GetStaticHtmlTest();
	void GetBinaryFileTest();
	void GetDirectoryWithTrailingSlashTest();
	void HeadRequestTest();

	// New tests
	void PostSimpleTextTest();
	void DeleteExistingFileTest();
	void GetCssFileTest();
	void GetImageFileTest();
	void GetLargeHtmlTest();
	void PostUploadBinaryFileTest();
	void PostOnRootTest();
	void GetAlternatePortTest();
	void DeleteBinaryFileTest();

	void AddAllTests();
public:
	HappyPathTests();
	~HappyPathTests();
};