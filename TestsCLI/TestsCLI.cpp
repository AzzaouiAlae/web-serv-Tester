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
		vector<int> choices = ReadChoices();
		HandleChoices(choices);
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
	cout << endl;
	cout << CLI::topLine() << endl;
	cout << CLI::row(string(CLR_TITLE) + SYM_STAR + " Overall Test Results" + RESET) << endl;
	cout << CLI::midLine() << endl;
	cout << CLI::row(string(CLR_DIM) + "Total: " + RESET + CLR_NAME + to_string(_testsCount) + RESET) << endl;
	cout << CLI::row(CLI::progressBar(_passedTests, _testsCount)) << endl;
	cout << CLI::row(string(CLR_PASS) + SYM_CHECK + " Passed: " + to_string(_passedTests) + RESET
		+ "    " + CLR_FAIL + SYM_CROSS + " Failed: " + to_string(_failedTests) + RESET) << endl;
	cout << CLI::botLine() << endl;
}

void TestCLI::PrintScreen()
{
	CLI::printBanner();
	cout << endl;
	cout << CLI::topLine() << endl;
	cout << CLI::row(string(CLR_TITLE) + SYM_GEAR + " Test Suites" + RESET) << endl;
	cout << CLI::midLine() << endl;
	cout << CLI::row(string(CLR_MENU_NUM) + "  0" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_PASS + "Run All Tests" + RESET) << endl;
	size_t i;
	for(i = 0; i < _testLists.size(); i++)
	{
		string num = to_string(i + 1);
		if (num.size() < 2) num = " " + num;
		cout << CLI::row(string(CLR_MENU_NUM) + "  " + num + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + _testLists[i]->getName() + RESET) << endl;
	}
	string exitNum = to_string(i + 1);
	if (exitNum.size() < 2) exitNum = " " + exitNum;
	cout << CLI::row(string(CLR_MENU_NUM) + "  " + exitNum + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_FAIL + "Exit" + RESET) << endl;
	cout << CLI::botLine() << endl;
}

vector<int> TestCLI::ReadChoices()
{
	cout << CLR_PROMPT << "  " << SYM_ARROW << " Choice (e.g. 1 3 5 or 1-5): " << RESET;
	string input = ATestList::readInput();
	return ATestList::parseChoices(input);
}

void TestCLI::RunSelectedTests(vector<int> &choices)
{
	_failedTests = 0;
	_passedTests = 0;
	for (size_t c = 0; c < choices.size(); c++)
	{
		int choice = choices[c];
		if (choice >= 1 && choice <= (int)_testLists.size())
		{
			_testLists[choice - 1]->RunAllTests();
			_failedTests += _testLists[choice - 1]->getFailedTests();
			_passedTests += _testLists[choice - 1]->getPassedTests();
		}
		else
			cerr << CLR_WARN << "  " << SYM_WARN << " Invalid choice: " << choice << RESET << endl;
	}
	_testsCount = _failedTests + _passedTests;
	PrintTestResult();
	ATestList::preperForNextTest();
}

void TestCLI::HandleChoices(vector<int> &choices)
{
	if (choices.empty())
		return;
	if (choices.size() > 1)
	{
		system("clear");
		RunSelectedTests(choices);
	}
	else
	{
		int choice = choices[0];
		if (choice == 0)
			RunAllTests();
		else if (choice == (int)_testLists.size() + 1)
		{
			cout << endl << CLR_DIM << "  Goodbye!" << RESET << endl << endl;
			exit(0);
		}
		else if (choice >= 1 && choice <= (int)_testLists.size())
		{
			system("clear");
			_testLists[choice - 1]->ShowTestsList();
		}
		else
			cerr << CLR_WARN << "  " << SYM_WARN << " Invalid choice: " << choice << RESET << endl;
	}
	system("clear");
}
