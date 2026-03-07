#pragma once
#include "../Headers.hpp"
#include "../ATestList/ATestList.hpp"
#include "../HandlerTestsList/HandlerTestsList.hpp"


class TestCLI {
	vector<ATestList *> _testLists;
	HandlerTestsList handlerTestsList;
	int _failedTests;
	int _passedTests;
	int _testsCount;
	void RunAllTests();
	void PrintScreen();
	void PrintTestResult();
public:
    TestCLI();
    ~TestCLI();
    void run();
};
