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
	cout << CLI::topLine() << endl;
	cout << CLI::row(string(CLR_NAME) + SYM_ARROW + " " + config.name + RESET) << endl;
	cout << CLI::midLine() << endl;
	CLI::printWrapped(cout, config.description, CLR_DIM);
	cout << CLI::botLine() << endl;
}

bool ATestList::connectToServer(TestCase &config)
{
	config.socket = Socket::inetConnect(config.host, config.port, SOCK_STREAM);
	if (config.socket == -1)
	{
		CLI::printError("Failed to connect to the server.");
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
			CLI::printError("Failed to wait for events.");
			return false;
		}
		else if (size == 0) {
			CLI::printError("Timeout while waiting for EpollOut events.");
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLOUT) == 0) {
			CLI::printError("Send Request Unexpected event type.");
			return false;
		}
		int sentBytes = config.socketIO->Send((void*)(config.request.c_str() + config.sendedBytes), config.request.size() - config.sendedBytes);
		if (config.socketIO->errorNumber) {
			CLI::printError("Failed to send request to the server.");
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
			CLI::printError("Failed to wait for events.");
			return false;
		}
		else if (size == 0) {
			CLI::printError("Timeout while waiting for EpollIn events.");
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLIN) == 0) {
			CLI::printError("Read Response Unexpected event type.");
			return false;
		}
		int receivedBytes = read(config.socketIO->GetFd(), config.responseBuffer, KBYTE);
		if (config.socketIO->errorNumber) {
			CLI::printError("Failed to receive response from the server.");
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
		cout << "  " << CLI::passBadge() << "  " << CLR_PASS << config.name << RESET << endl;
	}
	else {
		cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET << endl;
	}
	string responseHeader = config.response.substr(0, config.headerLength);
	printServerResponseHeader(config);
}

void ATestList::printServerResponseHeader(TestCase &config)
{
	string responseHeader = config.response.substr(0, config.headerLength);
	cout << endl;
	cout << CLI::topLine() << endl;
	cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " Server Response" + RESET) << endl;
	cout << CLI::midLine() << endl;
	CLI::printLines(cout, responseHeader, CLR_DIM);
	cout << CLI::midLine() << endl;
	CLI::printWrapped(cout, string("Expected: ") + config.expectedResponse, CLR_WARN);
	cout << CLI::botLine() << endl;
}

void ATestList::preperForNextTest()
{
	cout << endl;
	cout << CLR_PROMPT << "  " << SYM_ARROW << " Press Enter to continue..." << RESET;
	readInput();
	system("clear");
}

void ATestList::RunTestCase(TestCase &config)
{
	//act
	printTestCard(config);
	if (!connectToServer(config))
		return;
	if (!SendRequestToServer(config)) {
		multiplexer.DeleteFromEpoll(config.socketIO);
		return;
	}
	if (!ReadResponseFromServer(config)) {
		multiplexer.DeleteFromEpoll(config.socketIO);
		return;
	}
	//assert
	actServerResponse(config);
	multiplexer.DeleteFromEpoll(config.socketIO);
}

void ATestList::PrintTestResult()
{
	int total = (int)_testFunctions.size();
	cout << endl;
	cout << CLI::topLine() << endl;
	cout << CLI::row(string(CLR_TITLE) + SYM_STAR + " Test Results" + RESET) << endl;
	cout << CLI::midLine() << endl;
	cout << CLI::row(string(CLR_DIM) + "Total: " + RESET + CLR_NAME + to_string(total) + RESET) << endl;
	cout << CLI::row(CLI::progressBar(_passedTests, total)) << endl;
	cout << CLI::row(string(CLR_PASS) + SYM_CHECK + " Passed: " + to_string(_passedTests) + RESET
		+ "    " + CLR_FAIL + SYM_CROSS + " Failed: " + to_string(_failedTests) + RESET) << endl;
	cout << CLI::botLine() << endl;
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
		cerr << CLR_WARN << "  " << SYM_WARN << " Invalid input. Please enter a number." << RESET << endl;
		return readIntegerInput();
	} catch (const out_of_range&) {
		cerr << CLR_WARN << "  " << SYM_WARN << " Input out of range." << RESET << endl;
		return readIntegerInput();
	}
}

void ATestList::ShowTestsList()
{
	size_t choice;
	size_t i = 0;
	while (true)
	{
		cout << CLI::topLine() << endl;
		cout << CLI::row(string(CLR_MENU_CAT) + SYM_GEAR + " " + getName() + RESET) << endl;
		cout << CLI::midLine() << endl;
		cout << CLI::row(string(CLR_MENU_NUM) + "  0" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_PASS + "Run All Tests" + RESET) << endl;
		for (i = 0; i < _testFunctions.size(); i++)
		{
			string num = to_string(i + 1);
			if (num.size() < 2) num = " " + num;
			cout << CLI::row(string(CLR_MENU_NUM) + "  " + num + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + _testFunctions[i].first + RESET) << endl;
		}
		string retNum = to_string(i + 1);
		if (retNum.size() < 2) retNum = " " + retNum;
		cout << CLI::row(string(CLR_MENU_NUM) + "  " + retNum + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_WARN + "Return" + RESET) << endl;
		cout << CLI::botLine() << endl;
		cout << CLR_PROMPT << "  " << SYM_ARROW << " Choice: " << RESET;
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
		cout << endl << CLI::thickSeparator() << endl << endl;
	}
	
}

void ATestList::performTestCase(int choice)
{
	if (choice == 0) {
		RunAllTests();
		PrintTestResult();
	}
	else if (choice < 0 || choice > static_cast<int>(_testFunctions.size())) {
		cerr << CLR_WARN << "  " << SYM_WARN << " Invalid choice. Please try again." << RESET << endl;
	}
	else
	{
		(this->*(_testFunctions[choice - 1].second))();
	}
	preperForNextTest(); 
}