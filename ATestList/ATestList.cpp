#include "ATestList.hpp"

// Decompress chunked-encoded body
static bool decodeChunkedBodyForDisplay(const string &chunkedBody, string &decodedBody)
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
			return true;

		if (cursor + chunkSize + 2 > chunkedBody.size())
			return false;
		decodedBody.append(chunkedBody, cursor, chunkSize);
		cursor += chunkSize;
		if (chunkedBody.compare(cursor, 2, "\r\n") != 0)
			return false;
		cursor += 2;
	}
}

// Compress repeated characters for display (e.g., "YYYY..." becomes "Y repeated 1000000000 times")
static string compressRepeatedCharsForDisplay(const string &body, size_t maxDisplayLength = 100)
{
	if (body.empty())
		return body;

	// Check if body is mostly repetitions of a single character
	if (body.size() > maxDisplayLength)
	{
		char firstChar = body[0];
		bool isRepetitive = true;

		// Check a sample of the body to see if it's repetitive
		for (size_t i = 0; i < min(body.size(), (size_t)1000); ++i)
		{
			if (body[i] != firstChar)
			{
				isRepetitive = false;
				break;
			}
		}

		if (isRepetitive)
		{
			// Count how many times the character appears
			size_t count = 0;
			for (size_t i = 0; i < body.size() && body[i] == firstChar; ++i)
				++count;

			if (count == body.size())
			{
				stringstream result;
				result << "'" << firstChar << "' repeated " << body.size() << " times";
				return result.str();
			}
		}
	}

	return body;
}

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
	isListOfTests = false;
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

void failChild(TestCase &config, string failedPattern)
{
	if (config.childIndex != -1)
	{
		int fail = -(config.childIndex + 1);
		write(config.pipeFd[1], &fail, sizeof(int));
		CLI::printError("Child failed, " + failedPattern);
		sleep(7);
	}
}

bool ATestList::connectToServer(TestCase &config)
{
	config.socket = Socket::inetConnect(config.host, config.port, SOCK_STREAM);
	if (config.socket == -1)
	{
		failChild(config, "connectToServer");
		CLI::printError("Failed to connect to the server.");
		return false;
	}
	config.socketIO = new SocketIO(config.socket);
	return true;
}

static string toHex(size_t value)
{
	stringstream hexStream;
	hexStream << hex << value;
	return hexStream.str();
}

void ATestList::CreateChunkedBody(TestCase &config)
{
	config.chunkGenerated = config.bodyDescription.find("chunked") != string::npos;
	if (!config.chunkGenerated)
		return;

	char bodyChar = 'x';
	size_t charStart = config.bodyDescription.find("'");
	if (charStart != string::npos && charStart + 2 < config.bodyDescription.size())
	{
		bodyChar = config.bodyDescription[charStart + 1];
	}
	// Generate only ONE chunk
	config.chunkedBodyStart = generateChunkStartHeader(config.bodyDescription, config.chunkSize);
	config.chunkedBodyStart += generateBodySegment(bodyChar, config.chunkSize);
	config.chunkedBodyStart += "\r\n";

	// Calculate how many times this chunk repeats
	config.chunksRemaining = config.bodyTotalSize / config.chunkSize;

	// Handle remainder
	size_t remainder = config.bodyTotalSize % config.chunkSize;
	if (remainder)
	{
		config.chunkedBodyEnd = ::toHex(remainder) + "\r\n";
		config.chunkedBodyEnd += generateBodySegment(bodyChar, remainder);
		config.chunkedBodyEnd += "\r\n0\r\n\r\n";
	}
	else
	{
		config.chunkedBodyEnd = "0\r\n\r\n";
	}
}

int ATestList::SendChunkedBody(TestCase &config)
{
	int sentBytes = 0;

	// Phase 1: Send complete chunks repeatedly
	if (config.chunksRemaining > 0 && !config.sendingEndChunk)
	{
		size_t remainingInChunk = config.chunkedBodyStart.size() - config.chunkedBodyStartSentBytes;
		size_t toSend = (remainingInChunk > config.maxSend) ? config.maxSend : remainingInChunk;

		sentBytes = config.socketIO->Send(
			(void *)(config.chunkedBodyStart.c_str() + config.chunkedBodyStartSentBytes),
			toSend);

		if (sentBytes > 0)
		{
			config.chunkedBodyStartSentBytes += sentBytes;
			config.sendedBytes += sentBytes;

			// If we sent complete chunk, reset counter and decrement chunks remaining
			if (config.chunkedBodyStartSentBytes >= config.chunkedBodyStart.size())
			{
				config.chunkedBodyStartSentBytes = 0;
				config.chunksRemaining--;

				// If no more complete chunks, switch to sending end chunk
				if (config.chunksRemaining == 0)
				{
					config.sendingEndChunk = true;
				}
			}
		}

		return sentBytes;
	}

	// Phase 2: Send remainder chunk + terminator
	if (config.sendingEndChunk && config.chunkedBodyEndSentBytes < config.chunkedBodyEnd.size())
	{
		size_t remainingEnd = config.chunkedBodyEnd.size() - config.chunkedBodyEndSentBytes;
		size_t toSend = (remainingEnd > config.maxSend) ? config.maxSend : remainingEnd;

		sentBytes = config.socketIO->Send(
			(void *)(config.chunkedBodyEnd.c_str() + config.chunkedBodyEndSentBytes),
			toSend);

		if (sentBytes > 0)
		{
			config.chunkedBodyEndSentBytes += sentBytes;
			config.sendedBytes += sentBytes;
		}

		return sentBytes;
	}

	// All chunks sent
	return 0;
}

bool ATestList::SendRequestToServer(TestCase &config)
{
	multiplexer.AddAsEpollOut(config.socketIO);
	multiplexer.ChangeToEpollInOut(config.socketIO);
	CreateChunkedBody(config);

	size_t headerSize = config.request.size();
	bool headerSendComplete = false;

	while (true)
	{
		int size = multiplexer.epollWait(config.timeout);
		if (size == -1)
		{
			failChild(config, "epollWait -1");
			CLI::printError("Failed to wait for events.");
			return false;
		}
		else if (size == 0)
		{
			failChild(config, "epollWait timeout");
			CLI::printError("Timeout while waiting for EpollOut events.");
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLOUT) == 0)
		{
			return true;
		}

		int sentBytes = 0;

		// Phase 1: Send headers
		if (!headerSendComplete)
		{
			int toSend = headerSize - config.sendedBytes;
			if (toSend > (int)config.maxSend)
				toSend = (int)config.maxSend;

			sentBytes = config.socketIO->Send((void *)(config.request.c_str() + config.sendedBytes), toSend);
			if (config.socketIO->errorNumber)
			{
				failChild(config, "send header");
				CLI::printError("Failed to send request to the server.");
				return false;
			}
			if (sentBytes > 0)
			{
				config.sendedBytes += sentBytes;
			}

			if (config.sendedBytes >= headerSize)
			{
				headerSendComplete = true;
			}
		}
		else if (config.chunkGenerated)
		{
			// Phase 2: Send chunked body (start + end)
			sentBytes = SendChunkedBody(config);

			if (config.socketIO->errorNumber)
			{
				failChild(config, "send chunked body");
				CLI::printError("Failed to send chunked body.");
				return false;
			}

			// Check if all chunked body sent
			if (config.chunksRemaining == 0 && config.sendingEndChunk &&
				config.chunkedBodyEndSentBytes >= config.chunkedBodyEnd.size())
			{
				break;
			}
		}
		else
		{
			// No chunked body, headers sent - done
			return true;
		}

		if (isListOfTests == false && headerSendComplete)
		{
			system("clear");
			printTestCard(config);
			size_t totalSize = headerSize + config.bodyTotalSize;
			CLI::printHint("Sending request to server... (" + to_string(config.sendedBytes) + "/" + to_string(totalSize) + " bytes sent)");
		}
		usleep(config.sleepTime);
	}
	if (config.childIndex != -1)
	{	
		size_t totalSize = headerSize + config.bodyTotalSize;
		CLI::printHint("Child " + to_string(config.childIndex) + ": Sending request to server... (" + to_string(config.sendedBytes) + "/" + to_string(totalSize) + " bytes sent)");
	}
	return true;
}

bool ATestList::ReadResponseFromServer(TestCase &config)
{
	bool isHeadRequest = config.request.find("HEAD ") == 0;
	multiplexer.ChangeToEpollIn(config.socketIO);
	while (true)
	{
		int size = multiplexer.epollWait(config.timeout);
		if (size <= 0 && isHeadRequest &&
			GetResponseHeaderLength(config.response, config.headerLength))
		{
			break;
		}
		if (size == -1)
		{
			failChild(config, "epollWait -1");
			CLI::printError("Failed to wait for events.");
			return false;
		}
		else if (size == 0)
		{
			failChild(config, "epollWait timeout");
			CLI::printError("Timeout while waiting for EpollIn events.");
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLIN) == 0)
		{
			failChild(config, "Read Response Unexpected event type");
			CLI::printError("Read Response Unexpected event type.");
			return false;
		}
		int receivedBytes = read(config.socketIO->GetFd(), config.responseBuffer, KBYTE);
		if (receivedBytes == -1)
		{
			failChild(config, "Failed to receive response from the server");
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
	// Extract response body for pattern matching
	string responseBody;
	if (config.headerLength > 0 && config.headerLength <= config.response.size())
	{
		responseBody = config.response.substr(config.headerLength);
	}

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
			string subPattern = pattern.substr(pos, sep - pos);
			// Use smart pattern matching for repeated character patterns
			if (subPattern.find(" repeated ") != string::npos)
				matched = matchesBodyPattern(responseBody, subPattern);
			else
				matched = (config.response.find(subPattern) != string::npos);

			if (matched)
				break;
			pos = sep + 2;
		}

		if (!matched)
		{
			string finalPattern = pattern.substr(pos);
			// Use smart pattern matching for repeated character patterns
			if (finalPattern.find(" repeated ") != string::npos)
				matched = matchesBodyPattern(responseBody, finalPattern);
			else
				matched = (config.response.find(finalPattern) != string::npos);
		}

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
		if (config.printTest)
		{
			cout << "  " << CLI::passBadge() << "  " << CLR_PASS << config.name << RESET << endl;
		}
	}
	else
	{
		failChild(config, "Response did not match expected pattern: " + failedPattern);
		cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET;
		if (!failedPattern.empty())
			cerr << CLR_DIM << "  (missing: " << failedPattern << ")" << RESET;
		cerr << endl;
	}
	string responseHeader = config.response.substr(0, config.headerLength);
	if (config.printTest || config.passed == false)
	{
		printServerResponseHeader(config);
	}
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
			string bodyToDisplay = requestBody;
			// If request uses chunked encoding, decode it first before displaying
			if (toLowerCopy(requestHeader).find("transfer-encoding: chunked") != string::npos)
			{
				string decodedBody;
				if (decodeChunkedBodyForDisplay(requestBody, decodedBody))
					bodyToDisplay = decodedBody;
			}
			string compressedBody = compressRepeatedCharsForDisplay(bodyToDisplay);
			CLI::printLines(cout, compressedBody.empty() ? string("(empty)") : compressedBody, CLR_DIM);
		}
		else if (choice == "2")
		{
			cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " Server Body" + RESET) << endl;
			cout << CLI::midLine() << endl;
			string compressedBody = compressRepeatedCharsForDisplay(responseBody);
			CLI::printLines(cout, compressedBody.empty() ? string("(empty)") : compressedBody, CLR_DIM);
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

bool ATestList::matchesBodyPattern(const string &responseBody, const string &pattern)
{
	// Checks if responseBody matches pattern like "'K' repeated 100000000 times"
	// Extracts character and count from pattern and validates
	if (pattern.empty() || responseBody.empty())
		return pattern == responseBody;

	size_t repPos = pattern.find(" repeated ");
	size_t timesPos = pattern.find(" times");
	if (repPos == string::npos || timesPos == string::npos)
	{
		// Not a repeated pattern, use standard string matching
		return responseBody.find(pattern) != string::npos;
	}

	// Extract character (between quotes)
	size_t charStart = pattern.find("'");
	if (charStart == string::npos || charStart + 2 >= pattern.size())
		return responseBody.find(pattern) != string::npos;
	char expectedChar = pattern[charStart + 1];

	// Extract expected count
	size_t countStart = repPos + 9;
	size_t countEnd = timesPos;
	string countStr = pattern.substr(countStart, countEnd - countStart);
	size_t expectedCount = strtoull(countStr.c_str(), NULL, 10);

	// Count matches in response body
	if (responseBody.empty())
		return expectedCount == 0;

	// Check if first character matches
	if (responseBody[0] != expectedChar)
		return false;

	// Count consecutive matching characters
	size_t actualCount = 0;
	for (size_t i = 0; i < responseBody.size() && responseBody[i] == expectedChar; ++i)
		++actualCount;

	// If entire body is this character and count matches, it's valid
	return (actualCount == responseBody.size()) && (actualCount == expectedCount);
}

string ATestList::generateChunkStartHeader(const string &description, size_t &outChunkSize)
{
	// Parse description and generate the first chunk header
	// For chunked encoding format: "'K' repeated 100000000 times chunked size 32768"
	// Returns just the hex size line: "8000\r\n" (where 8000 is 32768 in hex)
	// outChunkSize is set to the chunk size extracted from description

	outChunkSize = 32768; // default

	if (description.empty())
		return "";

	// Check if chunked encoding requested
	if (description.find("chunked") == string::npos)
		return ""; // Not chunked, no header needed

	// Extract chunk size from description
	size_t sizePos = description.find(" size ");
	if (sizePos != string::npos)
	{
		string sizeStr = description.substr(sizePos + 6);
		outChunkSize = strtoull(sizeStr.c_str(), NULL, 10);
	}

	// Generate hex header for the chunk
	return ::toHex(outChunkSize) + "\r\n";
}

string ATestList::generateBodySegment(char bodyChar, size_t segmentSize)
{
	// Generate a segment of body data (raw characters, no HTTP framing)
	return string(segmentSize, bodyChar);
}

size_t ATestList::getBodyTotalSize(const string &description)
{
	// Parse "X repeated N times [chunked size S]" and return total body size
	if (description.empty())
		return 0;

	size_t repPos = description.find(" repeated ");
	size_t timesPos = description.find(" times");
	if (repPos == string::npos || timesPos == string::npos)
		return 0;

	size_t countStart = repPos + 9;
	size_t countEnd = timesPos;
	string countStr = description.substr(countStart, countEnd - countStart);
	size_t bodySize = strtoull(countStr.c_str(), NULL, 10);

	return bodySize;
}

void ATestList::RunTestCase(TestCase &config)
{
	// Reset tracking variables for new test
	config.sendedBytes = 0;
	config.chunkedBodyStartSentBytes = 0;
	config.chunkedBodyEndSentBytes = 0;
	config.chunksRemaining = 0;
	config.sendingEndChunk = false;
	config.chunkGenerated = false;

	// Setup body description for progressive generation
	if (!config.body.empty())
	{
		config.bodyDescription = config.body;
		config.bodyTotalSize = getBodyTotalSize(config.body);
		config.bodyGeneratedBytes = 0;
		config.isBodyGenerationComplete = false;
	}

	// act
	if (config.printTest)
	{
		printTestCard(config);
	}
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
			isListOfTests = true;
		}
		for (size_t c = 0; c < choices.size(); c++)
			performTestCase(choices[c]);
		if (multiChoice)
			PrintTestResult();
		isListOfTests = false;
		_showSingleTestDetails = false;
		preperForNextTest();
	}
}

void ATestList::RunAllTests()
{
	_showSingleTestDetails = false;
	ResetTestResults();
	isListOfTests = true;
	for (size_t i = 0; i < _testFunctions.size(); i++)
	{
		(this->*(_testFunctions[i].second))();
		cout << endl
			 << CLI::thickSeparator() << endl
			 << endl;
	}
	isListOfTests = false;
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

bool ATestList::createPipe(TestCase &config)
{
	// Create pipe for parent-child communication
	if (pipe(config.pipeFd) == -1)
	{
		cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET << " (pipe creation failed)" << endl;
		return false;
	}
	return true;
}

bool ATestList::forkChildProcess(TestCase &config)
{
	config.childIndex = 0;
	for (int i = 0; i < config.forkCount; i++, config.childIndex++)
	{
		pid_t pid = fork();
		if (pid == -1)
		{
			// Fork failed - kill all previously created children
			for (pid_t childPid : config.childPids)
			{
				kill(childPid, SIGTERM);
			}
			cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET << " (fork failed)" << endl;
			close(config.pipeFd[0]);
			close(config.pipeFd[1]);
			return false;
		}
		else if (pid != 0)
		{
			// Parent process - track child PID
			config.childPids.push_back(pid);
		}
		else
		{
			// Child process - close read end of pipe and return to run test case
			config.parentProcess = false;
			close(config.pipeFd[0]);
			return true;
		}
	}
	return true;
}

bool ATestList::runChildTestCase(TestCase &childConfig)
{
	int childSuccessCount = 0;
	int pass = 1;
	// int fail = -(childConfig.childIndex + 1);
	isListOfTests = true;
	multiplexer.epoolInit();
	close(childConfig.pipeFd[0]);
	for (int j = 0; j < childConfig.totalRequests; ++j)
	{
		childConfig.sendedBytes = 0;
		childConfig.response.clear();
		childConfig.headerLength = 0;
		childConfig.contentLength = 0;

		RunTestCase(childConfig);
		if (childConfig.passed)
		{
			write(childConfig.pipeFd[1], &pass, sizeof(int));
			childSuccessCount++;
		}

		multiplexer.DeleteFromEpoll(childConfig.socketIO);
		delete childConfig.socketIO;
	}

	write(childConfig.pipeFd[1], &childSuccessCount, sizeof(int));
	close(childConfig.pipeFd[1]);

	exit(childSuccessCount == childConfig.totalRequests ? 0 : 1);
}

void ATestList::getChildResults(TestCase &config)
{
	close(config.pipeFd[1]);

	int totalSuccessCount = 0;
	bool anyChildFailed = false;
	int failedChildIndex = -1;
	int totalExpected = config.forkCount * config.totalRequests;
	int percentStep;
	if (totalExpected > 1000)
		percentStep = totalExpected / 20;
	else if (totalExpected > 100)
		percentStep = totalExpected / 10;
	else
		percentStep = 1;
	for (int i = 0; i < totalExpected; i++)
	{
		int childSuccess = 0;
		ssize_t bytesRead = read(config.pipeFd[0], &childSuccess, sizeof(int));

		if (bytesRead > 0)
		{
			if (childSuccess <= 0)
			{
				anyChildFailed = true;
				if (childSuccess < 0)
					failedChildIndex = -childSuccess - 1;
				break;
			}
			else
			{
				totalSuccessCount++;
				if (isListOfTests == false && totalSuccessCount % percentStep == 0)
				{
					// system("clear");
					// printTestCard(config);
					CLI::printHint("request pass test... (" + to_string(totalSuccessCount) + "/" + to_string(totalExpected) + " bytes sent)");
				}
				else if (totalSuccessCount % percentStep == 0)
				{
					CLI::printHint("Progress: " + to_string(totalSuccessCount) + "/" + to_string(totalExpected) + " OK");
				}
			}
		}
		else
		{
			anyChildFailed = true;
		}
	}
	close(config.pipeFd[0]);

	if (anyChildFailed)
	{
		int status;
		pid_t childPid;
		for (size_t i = 0; i < config.childPids.size(); ++i)
		{
			childPid = config.childPids[i];
			if (failedChildIndex >= 0 && static_cast<int>(i) == failedChildIndex)
			{
				continue;
			}
			if (waitpid(childPid, &status, WNOHANG) == 0)
			{
				kill(childPid, SIGTERM);
				waitpid(childPid, &status, 0);
			}
		}
		if (failedChildIndex >= 0)
		{
			childPid = config.childPids[failedChildIndex];
			waitpid(childPid, &status, 0);
		}
	}
	else
	{
		int status;
		for (pid_t childPid : config.childPids)
		{
			waitpid(childPid, &status, 0);
		}
	}
	printForkChildTestResult(config, totalSuccessCount, anyChildFailed, failedChildIndex);
}

void ATestList::printForkChildTestResult(TestCase &config, int totalSuccessCount, bool anyChildFailed, int failedChildIndex)
{
	if (totalSuccessCount == config.totalRequests * config.forkCount && !anyChildFailed)
	{
		config.passed = true;
		_passedTests++;
		_failedTests--;
		cout << "  " << CLI::passBadge() << "  " << CLR_PASS << config.name << RESET
			 << " (" << totalSuccessCount << "/" << config.totalRequests * config.forkCount << " OK)" << endl;
	}
	else
	{
		cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET
			 << " (" << totalSuccessCount << "/" << config.totalRequests * config.forkCount << " OK)";
		if (failedChildIndex >= 0)
			cerr << CLR_DIM << "  [child " << failedChildIndex << " failed]" << RESET;
		cerr << endl;
	}
}

void ATestList::RunForkChildTestCase(TestCase &config)
{
	if (!createPipe(config))
	{
		return;
	}
	if (!forkChildProcess(config))
	{
		return;
	}
	if (!config.parentProcess)
	{
		runChildTestCase(config);
	}
	else
	{
		getChildResults(config);
	}
}
