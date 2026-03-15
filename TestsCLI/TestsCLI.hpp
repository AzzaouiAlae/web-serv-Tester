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
	void RunSelectedTests(vector<int> &choices);
	void PrintScreen();
	void PrintTestResult();
	vector<int> ReadChoices();
	void HandleChoices(vector<int> &choices);
public:
    TestCLI();
    ~TestCLI();
    void run();
};
