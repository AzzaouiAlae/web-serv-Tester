#pragma once
#include "../ATestList/ATestList.hpp"

class RoutingTests : public ATestList
{
	void RootMappingTest();
	void NestedPathResolutionTest();
	void TrailingSlashWithIndexTest();
	void NoTrailingSlashRedirectTest();
	void QueryStringIgnoredTest();
	void PercentEncodedPathTest();
	void AddAllTests();
public:
	RoutingTests();
	~RoutingTests();
};
