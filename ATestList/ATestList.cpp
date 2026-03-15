#include "ATestList.hpp"

namespace
{

	static string toLowerCopy(const string &value)
	{
		string lowered = value;
		for (size_t i = 0; i < lowered.size(); ++i)
			lowered[i] = (char)tolower((unsigned char)lowered[i]);
		return lowered;
	}

	static bool decodeChunkedBody(const string &chunkedBody, string &decodedBody)
	{
		decodedBody.clear();
		size_t cursor = 0;

		while (true)
		{
			size_t sizeLineEnd = chunkedBody.find("\r\n", cursor);
			if (sizeLineEnd == string::npos)
				return false;

			string sizeLine = chunkedBody.substr(cursor, sizeLineEnd - cursor);
			size_t extensionPos = sizeLine.find(';');
			if (extensionPos != string::npos)
				sizeLine = sizeLine.substr(0, extensionPos);
			if (sizeLine.empty())
				return false;

			char *endPtr = NULL;
			unsigned long chunkSize = strtoul(sizeLine.c_str(), &endPtr, 16);
			if (endPtr == NULL || *endPtr != '\0')
				return false;

			cursor = sizeLineEnd + 2;
			if (chunkSize == 0)
			{
				// Accept empty trailer section (\r\n) and trailer headers (..\r\n\r\n).
				if (cursor + 2 <= chunkedBody.size() && chunkedBody.compare(cursor, 2, "\r\n") == 0)
					return true;
				size_t trailerEnd = chunkedBody.find("\r\n\r\n", cursor);
				return trailerEnd != string::npos;
			}

			if (cursor + chunkSize + 2 > chunkedBody.size())
				return false;
			decodedBody.append(chunkedBody, cursor, chunkSize);
			cursor += chunkSize;
			if (chunkedBody.compare(cursor, 2, "\r\n") != 0)
				return false;
			cursor += 2;
		}
	}

	static void splitHttpMessage(const string &message, string &header, string &body)
	{
		size_t headerEnd = message.find("\r\n\r\n");
		if (headerEnd == string::npos)
		{
			header = message;
			body.clear();
			return;
		}
		header = message.substr(0, headerEnd + 4);
		body = message.substr(headerEnd + 4);
	}

} // namespace

vector<ATestList *> ATestList::_testLists;

ATestList::ATestList(const string &name) : _name(name), _showSingleTestDetails(false)
{
	_testLists.push_back(this);
}

string ATestList::getName() const
{
	return _name;
}

ATestList::~ATestList()
{
}

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
		if (size == -1)
		{
			CLI::printError("Failed to wait for events.");
			return false;
		}
		else if (size == 0)
		{
			CLI::printError("Timeout while waiting for EpollOut events.");
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLOUT) == 0)
		{
			CLI::printError("Send Request Unexpected event type.");
			return false;
		}
		int sentBytes = config.socketIO->Send((void *)(config.request.c_str() + config.sendedBytes), config.request.size() - config.sendedBytes);
		if (config.socketIO->errorNumber)
		{
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
		if (size == -1)
		{
			CLI::printError("Failed to wait for events.");
			return false;
		}
		else if (size == 0)
		{
			CLI::printError("Timeout while waiting for EpollIn events.");
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLIN) == 0)
		{
			CLI::printError("Read Response Unexpected event type.");
			return false;
		}
		int receivedBytes = read(config.socketIO->GetFd(), config.responseBuffer, KBYTE);
		if (receivedBytes == -1)
		{
			CLI::printError("Failed to receive response from the server.");
			return false;
		}
		else if (receivedBytes > 0)
			config.response.append(config.responseBuffer, receivedBytes);
		else if (receivedBytes == 0)
			break;
		if (GetResponseHeaderLength(config.response, config.headerLength))
		{
			if (IsChunkedTransferEncoding(config.response, config.headerLength))
			{
				if (HasChunkedTerminator(config.response, config.headerLength))
				{
					DecodeChunkedResponseBody(config.response, config.headerLength);
					config.contentLength = config.response.size() - config.headerLength;
					break;
				}
			}
			else if (GetContentLength(config.response, config.contentLength) && config.response.size() >= config.headerLength + config.contentLength)
			{
				break;
			}
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

bool ATestList::IsChunkedTransferEncoding(const string &response, size_t headerLength)
{
	if (headerLength > response.size())
		return false;
	string headers = toLowerCopy(response.substr(0, headerLength));
	return headers.find("transfer-encoding:") != string::npos && headers.find("chunked") != string::npos;
}

bool ATestList::HasChunkedTerminator(const string &response, size_t headerLength)
{
	if (headerLength > response.size())
		return false;
	return response.find("0\r\n\r\n", headerLength) != string::npos;
}

bool ATestList::DecodeChunkedResponseBody(string &response, size_t headerLength)
{
	if (headerLength > response.size())
		return false;
	string decodedBody;
	if (!decodeChunkedBody(response.substr(headerLength), decodedBody))
		return false;
	response = response.substr(0, headerLength) + decodedBody;
	return true;
}

void ATestList::rePrintTest(TestCase &config)
{
	printTestCard(config);
	if (config.passed)
		cout << "  " << CLI::passBadge() << "  " << CLR_PASS << config.name << RESET << endl;
	else
		cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET << endl;
}

void ATestList::actServerResponse(TestCase &config)
{
	bool allMatched = !config.expectedResponse.empty();
	string failedPattern;
	for (size_t i = 0; i < config.expectedResponse.size(); i++)
	{
		const string &pattern = config.expectedResponse[i];
		bool matched = false;
		// Support OR with "||": "patternA||patternB" passes if either matches
		size_t pos = 0;
		size_t sep;
		while ((sep = pattern.find("||", pos)) != string::npos)
		{
			if (config.response.find(pattern.substr(pos, sep - pos)) != string::npos)
			{
				matched = true;
				break;
			}
			pos = sep + 2;
		}
		if (!matched && config.response.find(pattern.substr(pos)) != string::npos)
			matched = true;
		if (!matched)
		{
			allMatched = false;
			failedPattern = pattern;
			break;
		}
	}
	if (allMatched)
	{
		config.passed = true;
		_passedTests++;
		_failedTests--;
		cout << "  " << CLI::passBadge() << "  " << CLR_PASS << config.name << RESET << endl;
	}
	else
	{
		cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET;
		if (!failedPattern.empty())
			cerr << CLR_DIM << "  (missing: " << failedPattern << ")" << RESET;
		cerr << endl;
	}
	string responseHeader = config.response.substr(0, config.headerLength);
	printServerResponseHeader(config);
}

long ATestList::CurrentTime()
{
	timeval time;
	gettimeofday(&time, NULL);
	return time.tv_sec * USEC + time.tv_usec;
}

string ATestList::GetRandem()
{
	srand(CurrentTime());
	return to_string(rand());
}

void ATestList::printServerResponseHeader(TestCase &config)
{
	string requestHeader;
	string requestBody;
	string responseHeader;
	string responseBody;

	splitHttpMessage(config.request, requestHeader, requestBody);
	if (config.headerLength <= config.response.size())
	{
		responseHeader = config.response.substr(0, config.headerLength);
		responseBody = config.response.substr(config.headerLength);
	}
	else
	{
		splitHttpMessage(config.response, responseHeader, responseBody);
	}
	while (true)
	{
		if (!_showSingleTestDetails)
		{
			cout << endl;
			cout << CLI::topLine() << endl;
			cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " Server Response" + RESET) << endl;
			cout << CLI::midLine() << endl;
			CLI::printLines(cout, responseHeader, CLR_DIM);
			cout << CLI::midLine() << endl;
			for (size_t i = 0; i < config.expectedResponse.size(); i++)
				CLI::printWrapped(cout, string("Expected: ") + config.expectedResponse[i], CLR_WARN);
			cout << CLI::botLine() << endl;
			return;
		}

		cout << endl;
		cout << CLI::topLine() << endl;
		cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " Tester Request Header" + RESET) << endl;
		cout << CLI::midLine() << endl;
		CLI::printLines(cout, requestHeader.empty() ? string("(empty)") : requestHeader, CLR_DIM);
		cout << CLI::midLine() << endl;
		cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " Server Response Header" + RESET) << endl;
		cout << CLI::midLine() << endl;
		CLI::printLines(cout, responseHeader.empty() ? string("(empty)") : responseHeader, CLR_DIM);
		cout << CLI::midLine() << endl;
		if (config.expectedResponse.empty())
			CLI::printWrapped(cout, "Expected: (empty)", CLR_WARN);
		else
		{
			for (size_t i = 0; i < config.expectedResponse.size(); i++)
				CLI::printWrapped(cout, string("Expected: ") + config.expectedResponse[i], CLR_WARN);
		}
		cout << CLI::midLine() << endl;
		cout << CLI::row(string(CLR_MENU_NUM) + "  1" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + "Print Tester Body" + RESET) << endl;
		cout << CLI::row(string(CLR_MENU_NUM) + "  2" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + "Print Server Body" + RESET) << endl;
		cout << CLI::row(string(CLR_MENU_NUM) + "  3" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + "Print configurationsForTestCase" + RESET) << endl;
		cout << CLI::botLine() << endl;

		cout << CLR_PROMPT << "  " << SYM_ARROW << " Detail option (1-3, Enter to return): " << RESET;
		string choice = readInput();
		if (choice.empty() || (choice != "1" && choice != "2" && choice != "3"))
			return;
		system("clear");
		cout << endl;
		cout << CLI::topLine() << endl;
		if (choice == "1")
		{
			cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " Tester Body" + RESET) << endl;
			cout << CLI::midLine() << endl;
			CLI::printLines(cout, requestBody.empty() ? string("(empty)") : requestBody, CLR_DIM);
		}
		else if (choice == "2")
		{
			cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " Server Body" + RESET) << endl;
			cout << CLI::midLine() << endl;
			CLI::printLines(cout, responseBody.empty() ? string("(empty)") : responseBody, CLR_DIM);
		}
		else
		{
			cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " configurationsForTestCase" + RESET) << endl;
			cout << CLI::midLine() << endl;
			CLI::printLines(cout, config.configurationsForTestCase.empty() ? string("(empty)") : config.configurationsForTestCase, CLR_DIM);
		}
		
		cout << CLI::botLine() << endl;
		cout << CLR_PROMPT << "  " << SYM_ARROW << " Press Enter to continue..." << RESET;
		readInput();
		system("clear");
		rePrintTest(config);
	}
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
	// act
	printTestCard(config);
	if (!connectToServer(config))
		return;
	if (!SendRequestToServer(config))
	{
		multiplexer.DeleteFromEpoll(config.socketIO);
		return;
	}
	if (!ReadResponseFromServer(config))
	{
		multiplexer.DeleteFromEpoll(config.socketIO);
		return;
	}
	// assert
	actServerResponse(config);
	multiplexer.DeleteFromEpoll(config.socketIO);
}

void ATestList::PrintTestResult()
{
	int total = _passedTests + _failedTests;
	cout << endl;
	cout << CLI::topLine() << endl;
	cout << CLI::row(string(CLR_TITLE) + SYM_STAR + " Test Results" + RESET) << endl;
	cout << CLI::midLine() << endl;
	cout << CLI::row(string(CLR_DIM) + "Total: " + RESET + CLR_NAME + to_string(total) + RESET) << endl;
	cout << CLI::row(CLI::progressBar(_passedTests, total)) << endl;
	cout << CLI::row(string(CLR_PASS) + SYM_CHECK + " Passed: " + to_string(_passedTests) + RESET + "    " + CLR_FAIL + SYM_CROSS + " Failed: " + to_string(_failedTests) + RESET) << endl;
	cout << CLI::botLine() << endl;
}

void ATestList::ResetTestResults()
{
	_failedTests = _testFunctions.size();
	_passedTests = 0;
}

string ATestList::readInput()
{
	string input;
	getline(cin, input);
	return input;
}

int ATestList::readIntegerInput()
{
	string input = readInput();
	try
	{
		return stoi(input);
	}
	catch (const invalid_argument &)
	{
		cerr << CLR_WARN << "  " << SYM_WARN << " Invalid input. Please enter a number." << RESET << endl;
		return readIntegerInput();
	}
	catch (const out_of_range &)
	{
		cerr << CLR_WARN << "  " << SYM_WARN << " Input out of range." << RESET << endl;
		return readIntegerInput();
	}
}

vector<int> ATestList::parseChoices(const string &input)
{
	vector<int> choices;
	istringstream iss(input);
	string token;
	while (iss >> token)
	{
		size_t dashPos = token.find('-');
		if (dashPos != string::npos && dashPos > 0 && dashPos < token.size() - 1)
		{
			try
			{
				int start = stoi(token.substr(0, dashPos));
				int end = stoi(token.substr(dashPos + 1));
				for (int i = start; i <= end; i++)
					choices.push_back(i);
			}
			catch (...)
			{
			}
		}
		else
		{
			try
			{
				choices.push_back(stoi(token));
			}
			catch (...)
			{
			}
		}
	}
	return choices;
}

void ATestList::ShowTestsList()
{
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
			if (num.size() < 2)
				num = " " + num;
			cout << CLI::row(string(CLR_MENU_NUM) + "  " + num + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + _testFunctions[i].first + RESET) << endl;
		}
		string retNum = to_string(i + 1);
		if (retNum.size() < 2)
			retNum = " " + retNum;
		cout << CLI::row(string(CLR_MENU_NUM) + "  " + retNum + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_WARN + "Return" + RESET) << endl;
		cout << CLI::botLine() << endl;
		cout << CLR_PROMPT << "  " << SYM_ARROW << " Choice (e.g. 1 3 5 or 1-5): " << RESET;
		string input = readInput();
		vector<int> choices = parseChoices(input);
		if (choices.size() == 1 && choices[0] == static_cast<int>(_testFunctions.size()) + 1)
		{
			break;
		}
		system("clear");
		bool multiChoice = choices.size() > 1;
		_showSingleTestDetails = !multiChoice && choices.size() == 1 && choices[0] >= 1 && choices[0] <= static_cast<int>(_testFunctions.size());
		if (multiChoice)
		{
			_failedTests = choices.size();
			_passedTests = 0;
		}
		for (size_t c = 0; c < choices.size(); c++)
			performTestCase(choices[c]);
		if (multiChoice)
			PrintTestResult();
		_showSingleTestDetails = false;
		preperForNextTest();
	}
}

void ATestList::RunAllTests()
{
	_showSingleTestDetails = false;
	ResetTestResults();
	for (size_t i = 0; i < _testFunctions.size(); i++)
	{
		(this->*(_testFunctions[i].second))();
		cout << endl
			 << CLI::thickSeparator() << endl
			 << endl;
	}
}

void ATestList::performTestCase(int choice)
{
	if (choice == 0)
	{
		RunAllTests();
		PrintTestResult();
	}
	else if (choice < 0 || choice > static_cast<int>(_testFunctions.size()))
	{
		cerr << CLR_WARN << "  " << SYM_WARN << " Invalid choice. Please try again." << RESET << endl;
	}
	else
	{
		(this->*(_testFunctions[choice - 1].second))();
	}
}