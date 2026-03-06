#pragma once
#include "../Headers.hpp"
#include "../ATestList/ATestList.hpp"
#include "../HandlerTestsList/HandlerTestsList.hpp"


class TestCLI {
	vector<ATestList *> _testLists;
	HandlerTestsList handlerTestsList;
public:
	
    TestCLI(int argc, char const *argv[]);
    ~TestCLI();

    void run();
	void PrintScreen();
	void initTests();

private:
    int _argc;
    char const **_argv;
	
};
