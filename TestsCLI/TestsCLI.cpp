#include "TestsCLI.hpp"

TestCLI::TestCLI(int argc, char const *argv[])
{
	_argc = argc;
	_argv = argv;
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

void TestCLI::PrintScreen()
{
	int choice = 0;
	cout << "Tests List:" << endl;
	cout << "------------------" << endl;
	size_t i;
	for(i = 0; i < _testLists.size(); i++)
	{
		cout << i + 1 << " " << _testLists[i]->getName() << endl;
	}
	cout << i + 1 << " Exit" << endl;
	cout << "------------------" << endl;
	cout << "Enter the number of the test list you want to run: ";
	cin >> choice;
	if(choice < 1 || choice > (int)_testLists.size() + 1)
	{
		cout << "Invalid choice" << endl;
		return;
	}
	if(choice == (int)_testLists.size() + 1)
	{
		cout << "Exiting..." << endl;
		exit(0);
	}
	system("clear");
	_testLists[choice - 1]->ShowTestsList();
}
