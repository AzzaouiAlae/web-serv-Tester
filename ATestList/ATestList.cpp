#include "ATestList.hpp"

vector<ATestList *> ATestList::_testLists;

ATestList::ATestList(const string &name): _name(name)
{
	nbTests = 0;
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
		cerr << "Failed to connect to the server." << endl;
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
			cerr << "Failed to wait for events." << endl;
			return false;
		}
		else if (size == 0) {
			cerr << "Timeout while waiting for EpollOut events." << endl;
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLOUT) == 0) {
			cerr << "Send Request Unexpected event type." << endl;
			return false;
		}
		int sentBytes = config.socketIO->Send((void*)(config.request.c_str() + config.sendedBytes), config.request.size() - config.sendedBytes);
		if (config.socketIO->errorNumber) {
			cerr << "Failed to send request to the server." << endl;
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
			cerr << "Failed to wait for events." << endl;
			return false;
		}
		else if (size == 0) {
			cerr << "Timeout while waiting for EpollIn events." << endl;
			return false;
		}
		else if ((multiplexer.eventList[0].events & EPOLLIN) == 0) {
			cerr << "Read Response Unexpected event type." << endl;
			return false;
		}
		int receivedBytes = read(config.socketIO->GetFd(), config.responseBuffer, KBYTE);
		if (config.socketIO->errorNumber) {
			cerr << "Failed to receive response from the server." << endl;
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
	if (config.response.find(config.expectedResponse) != string::npos)
		cout << config.name << " passed." << endl;
	else
		cerr << config.name << " failed." << endl;
	string responseHeader = config.response.substr(0, config.headerLength);
	printServerResponseHeader(responseHeader);
}

void ATestList::printServerResponseHeader(const string &response)
{
	cout << "Server Response:" << endl;
	cout << "------------------" << endl;
	cout << response << endl;
	cout << "------------------\n" << endl;
}

void ATestList::preperForNextTest()
{
	cout << "\n\n";
	cout << "Press Enter to continue to the next test...";
	cin.ignore();
	cin.get();
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
	preperForNextTest(); 
}
