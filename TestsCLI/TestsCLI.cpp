#include "TestsCLI.hpp"

TestCLI::TestCLI()
{
	_testLists = ATestList::getTestLists();
}

TestCLI::~TestCLI()
{}

void TestCLI::run()
{
	while (true)
	{
		PrintScreen();
	}
}

void TestCLI::RunAllTests()
{
	_failedTests = 0;
	_passedTests = 0;
	vector<ATestList *> testLists = ATestList::getTestLists();
	for (size_t i = 0; i < testLists.size(); i++)
	{
		testLists[i]->RunAllTests();
		_failedTests += testLists[i]->getFailedTests();
		_passedTests += testLists[i]->getPassedTests();
	}
	_testsCount = _failedTests + _passedTests;
	PrintTestResult();
	ATestList::preperForNextTest();
}

void TestCLI::PrintTestResult()
{
	cout << "\nTests Run: " << _testsCount << endl;
	cout << "Test Result: "
		<< "\033[32m" << _passedTests << " passed" << "\033[0m"
		<< ", "
		<< "\033[31m" << _failedTests << " failed" << "\033[0m"
		<< "." << endl;
}

void TestCLI::PrintScreen()
{
	int choice = 0;
	cout << "Tests List:" << endl;
	cout << "------------------" << endl;
	cout << "0. Run All Tests" << endl;
	size_t i;
	for(i = 0; i < _testLists.size(); i++)
	{
		cout << i + 1 << ". " << _testLists[i]->getName() << endl;
	}
	cout << i + 1 << ". Exit" << endl;
	cout << "------------------" << endl;
	cout << "Enter the number of the test list you want to run: ";
	choice = ATestList::readIntegerInput();
	if (choice == 0) {
		RunAllTests();
	}
	else if(choice < 1 || choice > (int)_testLists.size() + 1)
	{
		cout << "Invalid choice" << endl;
	}
	else if(choice == (int)_testLists.size() + 1)
	{
		cout << "Exiting..." << endl;
		exit(0);
	}
	else
	{
		system("clear");
		_testLists[choice - 1]->ShowTestsList();
	}
}
