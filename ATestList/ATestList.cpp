#include "ATestList.hpp"

vector<ATestList *> ATestList::_testLists;

ATestList::ATestList(const string &name): _name(name)
{
	_testLists.push_back(this);
}

string ATestList::getName() const
{
	return _name;
}

ATestList::~ATestList()
{}

vector<ATestList *> ATestList::getTestLists()
{
	return _testLists;
}

int ATestList::getFailedTests() const
{
	return _failedTests;
}

int ATestList::getPassedTests() const
{
	return _passedTests;
}

void ATestList::printTestCard(TestCase &config)
{
	cout << "Test: " << config.name << endl;
	cout << "Description: " << config.description << endl;
	cout << "------------------" << endl;
}

bool ATestList::connectToServer(TestCase &config)
{
	config.socket = Socket::inetConnect(config.host, config.port, SOCK_STREAM);
	if (config.socket == -1)
	{
		cerr << "\033[31m" << "Failed to connect to the server." << "\033[0m" << endl;
		return false;
	}
	config.socketIO = new SocketIO(config.socket);
	return true;
}

bool ATestList::SendRequestToServer(TestCase &config)
{
	multiplexer.AddAsEpollOut(config.socketIO);
	while (config.sendedBytes < config.request.size())
	{
		int size = multiplexer.epollWait(config.timeout);
		if (size == -1) {
			cerr << "\033[31m" << "Failed to wait for events." << "\033[0m" << endl;
			return false;
		}
		else if (size == 0) {
			cerr << "\033[31m" << "Timeout while waiting for EpollOut events." << "\033[0m" << endl;
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLOUT) == 0) {
			cerr << "\033[31m" << "Send Request Unexpected event type." << "\033[0m" << endl;
			return false;
		}
		int sentBytes = config.socketIO->Send((void*)(config.request.c_str() + config.sendedBytes), config.request.size() - config.sendedBytes);
		if (config.socketIO->errorNumber) {
			cerr << "\033[31m" << "Failed to send request to the server." << "\033[0m" << endl;
			return false;
		}
		else if (sentBytes > 0)
			config.sendedBytes += sentBytes;
	}
	return true;
}

bool ATestList::ReadResponseFromServer(TestCase &config)
{
	multiplexer.ChangeToEpollIn(config.socketIO);
	while (true)
	{
		int size = multiplexer.epollWait(config.timeout);
		if (size == -1) {
			cerr << "\033[31m" << "Failed to wait for events." << "\033[0m" << endl;
			return false;
		}
		else if (size == 0) {
			cerr << "\033[31m" << "Timeout while waiting for EpollIn events." << "\033[0m" << endl;
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLIN) == 0) {
			cerr << "\033[31m" << "Read Response Unexpected event type." << "\033[0m" << endl;
			return false;
		}
		int receivedBytes = read(config.socketIO->GetFd(), config.responseBuffer, KBYTE);
		if (config.socketIO->errorNumber) {
			cerr << "\033[31m" << "Failed to receive response from the server." << "\033[0m" << endl;
			return false;
		}
		else if (receivedBytes > 0)
			config.response.append(config.responseBuffer, receivedBytes);
		
		if (GetContentLength(config.response, config.contentLength) && GetResponseHeaderLength(config.response, config.headerLength))
		{
			if (config.response.size() >= config.headerLength + config.contentLength)
				break;
		}
	}
	multiplexer.DeleteFromEpoll(config.socketIO);
	return true;
}

bool ATestList::GetContentLength(string &response, size_t &contentLength)
{
	size_t pos = response.find("Content-Length:");
	if (pos != string::npos)
	{
		pos += strlen("Content-Length:");
		while (pos < response.size() && isspace(response[pos]))
			pos++;
		size_t endPos = response.find("\r\n", pos);
		if (endPos != string::npos)
		{
			string contentLengthStr = response.substr(pos, endPos - pos);
			contentLength = atoi(contentLengthStr.c_str());
		}
	}
	else
		return false;
	return true;
}

bool ATestList::GetResponseHeaderLength(string &response, size_t &headerLength)
{
	size_t pos = response.find("\r\n\r\n");
	if (pos != string::npos)
	{
		headerLength = pos + 4;
		return true;
	}
	return false;
}

void ATestList::actServerResponse(TestCase &config)
{
	if (config.response.find(config.expectedResponse) != string::npos) {
		config.passed = true;
		_passedTests++;
		_failedTests--;
		cout << "\033[32m" << config.name << " passed." << "\033[0m" << endl;
	}
	else {
		cerr << "\033[31m" << config.name << " failed." << "\033[0m" << endl;
	}
	string responseHeader = config.response.substr(0, config.headerLength);
	printServerResponseHeader(config);
}

void ATestList::printServerResponseHeader(TestCase &config)
{
	string responseHeader = config.response.substr(0, config.headerLength);
	cout << "Server Response:" << endl;
	cout << "------------------" << endl;
	cout << responseHeader;
	cout << "------------------" << endl;
	cout << "Expected Response: \n" << config.expectedResponse << endl;
}

void ATestList::preperForNextTest()
{
	cout << "\n";
	cout << "Press Enter to continue to the next test...";
	readInput();
	system("clear");
}

void ATestList::RunTestCase(TestCase &config)
{
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

void ATestList::PrintTestResult()
{
	cout << "\nTests Run: " << _testFunctions.size() << endl;
	cout << "Test Result: "
		<< "\033[32m" << _passedTests << " passed" << "\033[0m"
		<< ", "
		<< "\033[31m" << _failedTests << " failed" << "\033[0m"
		<< "." << endl;
}

void ATestList::ResetTestResults()
{
	_failedTests = _testFunctions.size();
	_passedTests = 0;
}

string  ATestList::readInput()
{
	string input;
	getline(cin, input);
	return input;
}

int ATestList::readIntegerInput()
{
	string input = readInput();
	try {
		return stoi(input);
	} catch (const invalid_argument&) {
		cerr << "Invalid input. Please enter a valid integer." << endl;
		return readIntegerInput();
	} catch (const out_of_range&) {
		cerr << "Input out of range. Please enter a valid integer." << endl;
		return readIntegerInput();
	}
}

void ATestList::ShowTestsList()
{
	size_t choice;
	size_t i = 0;
	while (true)
	{
		cout << getName() << ":" << endl;
		cout << "------------------" << endl;
		cout << "0. Run All Tests" << endl;
		for (i = 0; i < _testFunctions.size(); i++)
		{
			cout << i + 1 << ". " << _testFunctions[i].first << endl;
		}
		cout << i + 1 << ". Return" << endl;
		cout << "------------------" << endl;
		cout << "Enter your choice: ";
		choice = readIntegerInput();
		if (choice == _testFunctions.size() + 1) {
			break;
		}
		system("clear");
		performTestCase(choice);
	}
}

void ATestList::RunAllTests()
{
	ResetTestResults();
	for (size_t i = 0; i < _testFunctions.size(); i++)
	{
		(this->*(_testFunctions[i].second))();
		cout << "====================================\n" << endl;
	}
	
}

void ATestList::performTestCase(int choice)
{
	if (choice == 0) {
		RunAllTests();
		PrintTestResult();
	}
	else if (choice < 0 || choice > static_cast<int>(_testFunctions.size())) {
		cerr << "Invalid choice. Please try again." << endl;
	}
	else
	{
		(this->*(_testFunctions[choice - 1].second))();
	}
	preperForNextTest(); 
}