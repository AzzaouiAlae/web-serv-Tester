#pragma once
#include "../ATestList/ATestList.hpp"

class MethodRestrictionTests : public ATestList
{
	void PostToGetOnlyRouteTest();
	void DeleteToGetOnlyRouteTest();
	void GetToPostOnlyRouteTest();
	void DeleteToPostOnlyRouteTest();
	void AllowHeaderPresentOn405Test();
	void AddAllTests();
public:
	MethodRestrictionTests();
	~MethodRestrictionTests();
};
