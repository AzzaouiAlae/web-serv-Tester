#include "HappyPathTests.hpp"

HappyPathTests::HappyPathTests(): ATestList("Happy Path Tests")
{}

HappyPathTests::~HappyPathTests()
{}

void HappyPathTests::GetIndexTest()
{
	//arange
	TestConfig config;
	config.name = "GetIndexTest";
	config.description = "Test to check if the server returns the correct index page";
	config.port = "1025";
	config.host = "localhost";
	config.request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	config.expectedResponse = "HTTP/1.1 200 OK";
	config.configFileData = "";
	config.timeout = 2000;
	
	//act
	printTestCard(config);
	if (!connectToServer(config))
		return;
	if (!SendRequestToServer(config))
		return;
	if (!ReadResponseFromServer(config))
		return;

	//assert
	actServerResponse(config);
}

void HappyPathTests::GetStaticHtmlTest()
{
	//arrange
	TestConfig config;
	config.name = "GetStaticHtmlTest";
	config.description = "Test to check if the server returns a specific static HTML file directly";
	config.port = "1025";
	config.host = "localhost";
	// Explicitly asking for index.htm instead of just the / root
	config.request = "GET /index.htm HTTP/1.1\r\nHost: localhost\r\n\r\n";
	config.expectedResponse = "HTTP/1.1 200 OK"; 
	config.configFileData = "";
	config.timeout = 2000;
	
	//act
	printTestCard(config);
	if (!connectToServer(config))
		return;
	if (!SendRequestToServer(config))
		return;
	if (!ReadResponseFromServer(config))
		return;

	//assert
	actServerResponse(config);
}

void HappyPathTests::performTestCase(int choice)
{
	switch (choice) {
		case 1:
			GetIndexTest();
			break;
		case 2:
			GetStaticHtmlTest();
			break;
		case 3:
			cout << "Returning to main menu..." << endl;
			break;
		default:
			cout << "Invalid choice. Please try again." << endl;
			break;
	}
	// Wait for user input before clearing the screen
	cout << "Press Enter to continue...";
	cin.ignore();
	cin.get();
	system("clear");
}

void HappyPathTests::ShowTestsList()
{
	int choice;
	do {
		cout << "Happy Path Tests List:" << endl;
		cout << "------------------" << endl;
		cout << "1. GetIndexTest" << endl;
		cout << "2. GetStaticHtmlTest" << endl;
		cout << "3. Return" << endl;
		cout << "------------------" << endl;
		cout << "Enter your choice: ";
		cin >> choice;
		system("clear");
		performTestCase(choice);
	} while (choice != 3);
}
