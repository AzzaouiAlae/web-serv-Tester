#include "ATestList.hpp"
#include <cerrno>

// Decompress chunked-encoded body
// static bool decodeChunkedBodyForDisplay(const string &chunkedBody, string &decodedBody)
// {
// 	decodedBody.clear();
// 	size_t cursor = 0;

// 	while (true)
// 	{
// 		size_t sizeLineEnd = chunkedBody.find("\r\n", cursor);
// 		if (sizeLineEnd == string::npos)
// 			return false;

// 		string sizeLine = chunkedBody.substr(cursor, sizeLineEnd - cursor);
// 		size_t extensionPos = sizeLine.find(';');
// 		if (extensionPos != string::npos)
// 			sizeLine = sizeLine.substr(0, extensionPos);
// 		if (sizeLine.empty())
// 			return false;

// 		char *endPtr = NULL;
// 		unsigned long chunkSize = strtoul(sizeLine.c_str(), &endPtr, 16);
// 		if (endPtr == NULL || *endPtr != '\0')
// 			return false;

// 		cursor = sizeLineEnd + 2;
// 		if (chunkSize == 0)
// 			return true;

// 		if (cursor + chunkSize + 2 > chunkedBody.size())
// 			return false;
// 		decodedBody.append(chunkedBody, cursor, chunkSize);
// 		cursor += chunkSize;
// 		if (chunkedBody.compare(cursor, 2, "\r\n") != 0)
// 			return false;
// 		cursor += 2;
// 	}
// }

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

	static string trimCopy(const string &value)
	{
		size_t start = 0;
		while (start < value.size() && isspace((unsigned char)value[start]))
			++start;

		size_t end = value.size();
		while (end > start && isspace((unsigned char)value[end - 1]))
			--end;

		return value.substr(start, end - start);
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

	enum StreamingChunkParseState
	{
		STREAM_CHUNK_READ_SIZE,
		STREAM_CHUNK_READ_DATA,
		STREAM_CHUNK_EXPECT_DATA_CR,
		STREAM_CHUNK_EXPECT_DATA_LF,
		STREAM_CHUNK_READ_TRAILERS,
		STREAM_CHUNK_DONE
	};

	struct StreamingResponseState
	{
		string headerBuffer;
		bool headerComplete;
		vector<string> headerExpectedPatterns;
		bool bodyPatternEnabled;
		char bodyExpectedChar;
		size_t bodyExpectedCount;
		size_t bodyCount;
		bool chunkedResponse;
		bool hasContentLength;
		size_t contentLength;
		size_t decodedBodyBytes;
		bool responseComplete;
		StreamingChunkParseState chunkState;
		string chunkLineBuffer;
		size_t chunkBytesRemaining;
		string trailerBuffer;

		StreamingResponseState() : headerComplete(false), bodyPatternEnabled(false), bodyExpectedChar('\0'),
								 bodyExpectedCount(0), bodyCount(0), chunkedResponse(false), hasContentLength(false),
								 contentLength(0), decodedBodyBytes(0), responseComplete(false),
								 chunkState(STREAM_CHUNK_READ_SIZE), chunkBytesRemaining(0)
		{
		}
	};

	struct StreamingSendState
	{
		size_t headerSize;
		size_t headerSentBytes;
		bool headerSendComplete;
		bool hasGeneratedBody;
		bool requestSendComplete;
		bool writeShutdownSent;
		string bodySegment;
		size_t totalSendSize;
		size_t payloadBytesPlanned;

		StreamingSendState() : headerSize(0), headerSentBytes(0), headerSendComplete(false),
						   hasGeneratedBody(false), requestSendComplete(false),
						   writeShutdownSent(false), totalSendSize(0), payloadBytesPlanned(0)
		{
		}
	};

	static bool isWouldBlockError()
	{
		return errno == EAGAIN || errno == EWOULDBLOCK;
	}

	static bool isPeerClosedWriteError()
	{
		return errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN;
	}

	static bool expectsPayloadTooLargeResponse(const TestCase &config)
	{
		for (size_t i = 0; i < config.expectedResponse.size(); ++i)
		{
			string lowered = toLowerCopy(config.expectedResponse[i]);
			if (lowered.find("413") != string::npos ||
				lowered.find("payload too large") != string::npos ||
				lowered.find("entity too large") != string::npos ||
				lowered.find("content too large") != string::npos)
				return true;
		}
		return false;
	}

	static bool shouldAcceptEarlyCloseDuringUpload(const TestCase &config)
	{
		return isPeerClosedWriteError() && expectsPayloadTooLargeResponse(config);
	}

	static bool setSocketNonBlocking(int fd)
	{
		int flags = fcntl(fd, F_GETFL, 0);
		if (flags == -1)
			return false;
		if ((flags & O_NONBLOCK) != 0)
			return true;
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
	}

	static bool sendSocketData(SocketIO *socketIO, const char *buffer, size_t length, int &sentBytes)
	{
		sentBytes = socketIO->Send((void *)buffer, length);
		if (sentBytes == -1 && isWouldBlockError())
		{
			sentBytes = 0;
			return true;
		}
		return sentBytes >= 0;
	}

	// Splits a header-shaped expectation ("Content-Type: text/html", "Allow:")
	// into a lowercased field name and value. Anything else (status lines, body
	// sentinels) is rejected so it keeps being matched literally.
	static bool splitHeaderPattern(const string &pattern, string &name, string &value)
	{
		size_t colon = pattern.find(':');
		if (colon == string::npos || colon == 0)
			return false;

		for (size_t i = 0; i < colon; ++i)
		{
			char c = pattern[i];
			if (!isalnum((unsigned char)c) && c != '-' && c != '_')
				return false;
		}

		name = toLowerCopy(pattern.substr(0, colon));
		value = toLowerCopy(trimCopy(pattern.substr(colon + 1)));
		return true;
	}

	// Field names are case-insensitive and the whitespace after the colon is not
	// fixed, so "Content-Type: text/html" must also match "content-type:TEXT/HTML".
	static bool matchesHeaderField(const string &header, const string &name, const string &value)
	{
		string lowered = toLowerCopy(header);
		size_t lineStart = 0;

		while (lineStart < lowered.size())
		{
			size_t lineEnd = lowered.find('\n', lineStart);
			string line = (lineEnd == string::npos) ? lowered.substr(lineStart)
													: lowered.substr(lineStart, lineEnd - lineStart);

			size_t colon = line.find(':');
			if (colon != string::npos && trimCopy(line.substr(0, colon)) == name &&
				trimCopy(line.substr(colon + 1)).find(value) != string::npos)
				return true;

			if (lineEnd == string::npos)
				break;
			lineStart = lineEnd + 1;
		}
		return false;
	}

	static bool containsPattern(const string &haystack, const string &pattern)
	{
		if (haystack.find(pattern) != string::npos)
			return true;

		string name;
		string value;
		if (!splitHeaderPattern(pattern, name, value))
			return false;

		// Only the header section is rescanned case-insensitively: lowercasing a
		// multi-megabyte body to look for a header field would be wasted work.
		size_t headerEnd = haystack.find("\r\n\r\n");
		if (headerEnd == string::npos)
			return matchesHeaderField(haystack, name, value);
		return matchesHeaderField(haystack.substr(0, headerEnd + 2), name, value);
	}

	static bool matchAnyOrPattern(const string &haystack, const string &pattern)
	{
		size_t pos = 0;
		while (true)
		{
			size_t sep = pattern.find("||", pos);
			string candidate = (sep == string::npos) ? pattern.substr(pos) : pattern.substr(pos, sep - pos);
			if (containsPattern(haystack, candidate))
				return true;
			if (sep == string::npos)
				break;
			pos = sep + 2;
		}
		return false;
	}

	static bool parseRepeatedBodyPattern(const string &pattern, char &expectedChar, size_t &expectedCount)
	{
		size_t repeatedPos = pattern.find(" repeated ");
		size_t timesPos = pattern.find(" times");
		if (repeatedPos == string::npos || timesPos == string::npos || timesPos <= repeatedPos + 9)
			return false;

		size_t quoteStart = pattern.find("'");
		if (quoteStart == string::npos || quoteStart + 2 >= pattern.size() || pattern[quoteStart + 2] != '\'')
			return false;
		expectedChar = pattern[quoteStart + 1];

		string countStr = pattern.substr(repeatedPos + 9, timesPos - (repeatedPos + 9));
		if (countStr.empty())
			return false;

		char *endPtr = NULL;
		unsigned long long parsed = strtoull(countStr.c_str(), &endPtr, 10);
		if (endPtr == NULL || *endPtr != '\0')
			return false;

		expectedCount = static_cast<size_t>(parsed);
		return true;
	}

	static void buildStreamingExpectations(const TestCase &config, StreamingResponseState &state)
	{
		for (size_t i = 0; i < config.expectedResponse.size(); ++i)
		{
			const string &pattern = config.expectedResponse[i];
			char repeatedChar = '\0';
			size_t repeatedCount = 0;
			if (!state.bodyPatternEnabled &&
				pattern.find(" repeated ") != string::npos &&
				parseRepeatedBodyPattern(pattern, repeatedChar, repeatedCount))
			{
				state.bodyPatternEnabled = true;
				state.bodyExpectedChar = repeatedChar;
				state.bodyExpectedCount = repeatedCount;
			}
			else
			{
				state.headerExpectedPatterns.push_back(pattern);
			}
		}
	}

	static bool validateHeaderPatterns(const string &header, const vector<string> &patterns, string &failReason)
	{
		for (size_t i = 0; i < patterns.size(); ++i)
		{
			if (!matchAnyOrPattern(header, patterns[i]))
			{
				failReason = string("Response did not match expected pattern: ") + patterns[i];
				return false;
			}
		}
		return true;
	}

	static bool parseContentLengthHeader(const string &header, size_t &contentLength)
	{
		string lowered = toLowerCopy(header);
		size_t pos = lowered.find("content-length:");
		if (pos == string::npos)
			return false;

		pos += strlen("content-length:");
		while (pos < lowered.size() && isspace((unsigned char)lowered[pos]))
			++pos;
		size_t endPos = lowered.find("\r\n", pos);
		if (endPos == string::npos)
			return false;

		string lengthStr = lowered.substr(pos, endPos - pos);
		if (lengthStr.empty())
			return false;

		char *endPtr = NULL;
		unsigned long long parsed = strtoull(lengthStr.c_str(), &endPtr, 10);
		if (endPtr == NULL || *endPtr != '\0')
			return false;

		contentLength = static_cast<size_t>(parsed);
		return true;
	}

	static bool hasChunkedTransferEncoding(const string &header)
	{
		string lowered = toLowerCopy(header);
		return lowered.find("transfer-encoding:") != string::npos && lowered.find("chunked") != string::npos;
	}

	static bool parseHttpStatusCode(const string &header, int &statusCode)
	{
		statusCode = 0;
		size_t lineEnd = header.find("\r\n");
		string statusLine = (lineEnd == string::npos) ? header : header.substr(0, lineEnd);

		size_t firstSpace = statusLine.find(' ');
		if (firstSpace == string::npos)
			return false;
		while (firstSpace < statusLine.size() && statusLine[firstSpace] == ' ')
			++firstSpace;
		if (firstSpace >= statusLine.size())
			return false;

		size_t codeEnd = firstSpace;
		while (codeEnd < statusLine.size() && isdigit((unsigned char)statusLine[codeEnd]))
			++codeEnd;
		if (codeEnd == firstSpace)
			return false;

		string codeStr = statusLine.substr(firstSpace, codeEnd - firstSpace);
		statusCode = atoi(codeStr.c_str());
		return statusCode >= 100;
	}

	static bool extractHeaderValue(const string &header, const string &name, string &value)
	{
		string loweredHeader = toLowerCopy(header);
		string loweredName = toLowerCopy(name) + ":";

		size_t cursor = 0;
		while (true)
		{
			size_t found = loweredHeader.find(loweredName, cursor);
			if (found == string::npos)
				return false;

			bool isLineStart = (found == 0) || (loweredHeader[found - 1] == '\n');
			if (!isLineStart)
			{
				cursor = found + loweredName.size();
				continue;
			}

			size_t valueStart = found + loweredName.size();
			while (valueStart < header.size() && (header[valueStart] == ' ' || header[valueStart] == '\t'))
				++valueStart;

			size_t valueEnd = header.find("\r\n", valueStart);
			if (valueEnd == string::npos)
				valueEnd = header.size();

			value = trimCopy(header.substr(valueStart, valueEnd - valueStart));
			return !value.empty();
		}
	}

	static bool isLoopbackHost(const string &host)
	{
		return host == "localhost" || host == "127.0.0.1" || host == "::1";
	}

	static bool areEquivalentRedirectHosts(const string &lhs, const string &rhs)
	{
		string loweredLhs = toLowerCopy(lhs);
		string loweredRhs = toLowerCopy(rhs);
		if (loweredLhs == loweredRhs)
			return true;
		return isLoopbackHost(loweredLhs) && isLoopbackHost(loweredRhs);
	}

	static bool parseRedirectTarget(const TestCase &config,
								 const string &location,
								 string &targetHost,
								 string &targetPort,
								 string &targetPath)
	{
		targetHost = config.host;
		targetPort = config.port;
		targetPath.clear();

		string cleanedLocation = trimCopy(location);
		if (cleanedLocation.empty())
			return false;

		if (cleanedLocation.compare(0, 8, "https://") == 0)
			return false;

		if (cleanedLocation.compare(0, 7, "http://") == 0)
		{
			size_t authorityStart = 7;
			size_t pathStart = cleanedLocation.find('/', authorityStart);
			string authority = (pathStart == string::npos)
				? cleanedLocation.substr(authorityStart)
				: cleanedLocation.substr(authorityStart, pathStart - authorityStart);

			size_t userInfoPos = authority.rfind('@');
			if (userInfoPos != string::npos)
				authority = authority.substr(userInfoPos + 1);
			if (authority.empty())
				return false;

			string parsedHost;
			string parsedPort;
			if (authority[0] == '[')
			{
				size_t bracketEnd = authority.find(']');
				if (bracketEnd == string::npos)
					return false;
				parsedHost = authority.substr(1, bracketEnd - 1);
				if (bracketEnd + 1 < authority.size())
				{
					if (authority[bracketEnd + 1] != ':')
						return false;
					parsedPort = authority.substr(bracketEnd + 2);
				}
			}
			else
			{
				size_t lastColon = authority.rfind(':');
				if (lastColon != string::npos && authority.find(':') == lastColon)
				{
					parsedHost = authority.substr(0, lastColon);
					parsedPort = authority.substr(lastColon + 1);
				}
				else
					parsedHost = authority;
			}

			if (parsedHost.empty())
				return false;
			if (!areEquivalentRedirectHosts(config.host, parsedHost))
				return false;

			targetHost = parsedHost;
			targetPort = parsedPort.empty() ? string("80") : parsedPort;
			targetPath = (pathStart == string::npos) ? string("/") : cleanedLocation.substr(pathStart);
			return true;
		}

		targetPath = cleanedLocation;
		if (targetPath[0] != '/')
			targetPath = string("/") + targetPath;
		return true;
	}

	static bool sendAllBytes(int fd, const string &payload)
	{
		size_t sentTotal = 0;
		while (sentTotal < payload.size())
		{
			ssize_t sent = write(fd, payload.c_str() + sentTotal, payload.size() - sentTotal);
			if (sent == -1)
			{
				if (errno == EINTR)
					continue;
				return false;
			}
			if (sent == 0)
				return false;
			sentTotal += static_cast<size_t>(sent);
		}
		return true;
	}

	static void applySocketTimeout(int fd, int timeoutMs)
	{
		if (timeoutMs <= 0)
			return;

		timeval tv;
		tv.tv_sec = timeoutMs / 1000;
		tv.tv_usec = (timeoutMs % 1000) * 1000;
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	}

	static bool readHttpResponseFromSocket(int fd, string &response, size_t &headerLength, size_t &contentLength)
	{
		response.clear();
		headerLength = 0;
		contentLength = 0;

		bool headerParsed = false;
		bool chunked = false;
		bool hasContentLengthValue = false;
		char buffer[8192];

		while (true)
		{
			ssize_t received = read(fd, buffer, sizeof(buffer));
			if (received == -1)
			{
				if (errno == EINTR)
					continue;
				return false;
			}
			if (received == 0)
				break;

			response.append(buffer, static_cast<size_t>(received));
			if (!headerParsed)
			{
				size_t headerEnd = response.find("\r\n\r\n");
				if (headerEnd != string::npos)
				{
					headerParsed = true;
					headerLength = headerEnd + 4;
					string header = response.substr(0, headerLength);
					chunked = hasChunkedTransferEncoding(header);
					hasContentLengthValue = parseContentLengthHeader(header, contentLength);
				}
			}

			if (!headerParsed)
				continue;

			if (chunked)
			{
				string decodedBody;
				if (decodeChunkedBody(response.substr(headerLength), decodedBody))
				{
					response = response.substr(0, headerLength) + decodedBody;
					contentLength = decodedBody.size();
					return true;
				}
			}
			else if (hasContentLengthValue)
			{
				size_t expectedTotal = headerLength + contentLength;
				if (response.size() >= expectedTotal)
				{
					if (response.size() > expectedTotal)
						response.erase(expectedTotal);
					return true;
				}
			}
		}

		if (!headerParsed)
			return false;

		if (chunked)
		{
			string decodedBody;
			if (!decodeChunkedBody(response.substr(headerLength), decodedBody))
				return false;
			response = response.substr(0, headerLength) + decodedBody;
			contentLength = decodedBody.size();
			return true;
		}

		if (hasContentLengthValue)
			return response.size() >= headerLength + contentLength;

		contentLength = response.size() - headerLength;
		return true;
	}

	static bool patternMatchesResponse(const string &response, const string &responseBody, const string &pattern)
	{
		if (pattern.find(" repeated ") != string::npos)
		{
			char expectedChar = '\0';
			size_t expectedCount = 0;
			if (parseRepeatedBodyPattern(pattern, expectedChar, expectedCount))
			{
				if (responseBody.size() != expectedCount)
					return false;
				for (size_t i = 0; i < responseBody.size(); ++i)
				{
					if (responseBody[i] != expectedChar)
						return false;
				}
				return true;
			}

			return responseBody.find(pattern) != string::npos;
		}

		return containsPattern(response, pattern);
	}

	static bool validateExpectedPatternsAgainstResponse(const string &response,
											const string &responseBody,
											const vector<string> &expectedResponse,
											string &failedPattern)
	{
		if (expectedResponse.empty())
			return false;

		for (size_t i = 0; i < expectedResponse.size(); ++i)
		{
			const string &pattern = expectedResponse[i];
			bool matched = false;
			size_t pos = 0;

			while (true)
			{
				size_t sep = pattern.find("||", pos);
				string candidate = (sep == string::npos) ? pattern.substr(pos) : pattern.substr(pos, sep - pos);

				if (patternMatchesResponse(response, responseBody, candidate))
				{
					matched = true;
					break;
				}

				if (sep == string::npos)
					break;
				pos = sep + 2;
			}

			if (!matched)
			{
				failedPattern = pattern;
				return false;
			}
		}

		return true;
	}

	static bool followRedirectWithGet(TestCase &config, string &followedLocation)
	{
		string responseHeader;
		string responseBody;
		splitHttpMessage(config.response, responseHeader, responseBody);
		if (responseHeader.empty())
			return false;

		int statusCode = 0;
		if (!parseHttpStatusCode(responseHeader, statusCode) || statusCode < 300 || statusCode >= 400)
			return false;

		string location;
		if (!extractHeaderValue(responseHeader, "Location", location))
			return false;

		string redirectHost;
		string redirectPort;
		string redirectPath;
		if (!parseRedirectTarget(config, location, redirectHost, redirectPort, redirectPath))
			return false;

		int redirectSocket = Socket::inetConnect(redirectHost, redirectPort, SOCK_STREAM);
		if (redirectSocket == -1)
			return false;

		applySocketTimeout(redirectSocket, config.timeout);

		string hostHeader = redirectHost;
		if (!redirectPort.empty() && redirectPort != "80")
			hostHeader += ":" + redirectPort;

		string getRequest =
			"GET " + redirectPath + " HTTP/1.1\r\n"
			"Host: " + hostHeader + "\r\n"
			"Connection: close\r\n"
			"\r\n";

		bool success = sendAllBytes(redirectSocket, getRequest);
		if (success)
			shutdown(redirectSocket, SHUT_WR);

		string redirectedResponse;
		size_t redirectedHeaderLength = 0;
		size_t redirectedContentLength = 0;
		if (success)
			success = readHttpResponseFromSocket(redirectSocket,
											 redirectedResponse,
											 redirectedHeaderLength,
											 redirectedContentLength);

		close(redirectSocket);
		if (!success)
			return false;

		config.response = redirectedResponse;
		config.headerLength = redirectedHeaderLength;
		config.contentLength = redirectedContentLength;
		followedLocation = location;
		return true;
	}

	static bool validateStreamingBodyBytes(StreamingResponseState &state, const char *data, size_t length, string &failReason)
	{
		if (!state.bodyPatternEnabled)
			return true;

		for (size_t i = 0; i < length; ++i)
		{
			if (data[i] != state.bodyExpectedChar)
			{
				failReason = string("Body mismatch: expected only '") + state.bodyExpectedChar + "' characters.";
				return false;
			}
		}

		if (state.bodyCount + length > state.bodyExpectedCount)
		{
			size_t observedAtLeast = state.bodyCount + length;
			state.bodyCount = observedAtLeast;
			state.decodedBodyBytes += length;
			failReason = "Body larger than expected repeated-count pattern (expected " +
				to_string(state.bodyExpectedCount) + ", received at least " +
				to_string(observedAtLeast) + ").";
			return false;
		}

		state.bodyCount += length;
		return true;
	}

	static bool parseChunkSizeLine(const string &sizeLineRaw, size_t &chunkSize)
	{
		string sizeLine = sizeLineRaw;
		size_t extensionPos = sizeLine.find(';');
		if (extensionPos != string::npos)
			sizeLine = sizeLine.substr(0, extensionPos);
		if (sizeLine.empty())
			return false;

		char *endPtr = NULL;
		unsigned long long parsed = strtoull(sizeLine.c_str(), &endPtr, 16);
		if (endPtr == NULL || *endPtr != '\0')
			return false;

		chunkSize = static_cast<size_t>(parsed);
		return true;
	}

	static bool consumeIdentityBody(const char *data, size_t length, StreamingResponseState &state, string &failReason)
	{
		if (!validateStreamingBodyBytes(state, data, length, failReason))
			return false;

		state.decodedBodyBytes += length;
		if (state.hasContentLength)
		{
			if (state.decodedBodyBytes > state.contentLength)
			{
				failReason = "Body larger than declared Content-Length.";
				return false;
			}
			if (state.decodedBodyBytes == state.contentLength)
				state.responseComplete = true;
		}

		return true;
	}

	static bool consumeChunkedBody(const char *data, size_t length, StreamingResponseState &state, string &failReason)
	{
		size_t cursor = 0;
		while (cursor < length)
		{
			if (state.chunkState == STREAM_CHUNK_DONE)
			{
				failReason = "Received bytes after final chunk terminator.";
				return false;
			}

			if (state.chunkState == STREAM_CHUNK_READ_SIZE)
			{
				state.chunkLineBuffer.push_back(data[cursor++]);
				size_t lineLen = state.chunkLineBuffer.size();
				if (lineLen >= 2 && state.chunkLineBuffer[lineLen - 2] == '\r' && state.chunkLineBuffer[lineLen - 1] == '\n')
				{
					string line = state.chunkLineBuffer.substr(0, lineLen - 2);
					state.chunkLineBuffer.clear();
					if (!parseChunkSizeLine(line, state.chunkBytesRemaining))
					{
						failReason = "Invalid chunk size in response body.";
						return false;
					}
					if (state.chunkBytesRemaining == 0)
					{
						state.chunkState = STREAM_CHUNK_READ_TRAILERS;
						state.trailerBuffer.clear();
					}
					else
						state.chunkState = STREAM_CHUNK_READ_DATA;
				}
				continue;
			}

			if (state.chunkState == STREAM_CHUNK_READ_DATA)
			{
				size_t available = length - cursor;
				size_t toConsume = min(available, state.chunkBytesRemaining);
				if (!validateStreamingBodyBytes(state, data + cursor, toConsume, failReason))
					return false;
				state.decodedBodyBytes += toConsume;
				state.chunkBytesRemaining -= toConsume;
				cursor += toConsume;
				if (state.chunkBytesRemaining == 0)
					state.chunkState = STREAM_CHUNK_EXPECT_DATA_CR;
				continue;
			}

			if (state.chunkState == STREAM_CHUNK_EXPECT_DATA_CR)
			{
				if (data[cursor] != '\r')
				{
					failReason = "Invalid chunk delimiter (expected CR).";
					return false;
				}
				++cursor;
				state.chunkState = STREAM_CHUNK_EXPECT_DATA_LF;
				continue;
			}

			if (state.chunkState == STREAM_CHUNK_EXPECT_DATA_LF)
			{
				if (data[cursor] != '\n')
				{
					failReason = "Invalid chunk delimiter (expected LF).";
					return false;
				}
				++cursor;
				state.chunkState = STREAM_CHUNK_READ_SIZE;
				continue;
			}

			if (state.chunkState == STREAM_CHUNK_READ_TRAILERS)
			{
				state.trailerBuffer.push_back(data[cursor++]);
				size_t trailerLen = state.trailerBuffer.size();
				if (state.trailerBuffer == "\r\n")
				{
					state.chunkState = STREAM_CHUNK_DONE;
					state.responseComplete = true;
					continue;
				}
				if (trailerLen >= 4 && state.trailerBuffer.substr(trailerLen - 4) == "\r\n\r\n")
				{
					state.chunkState = STREAM_CHUNK_DONE;
					state.responseComplete = true;
					continue;
				}
			}
		}

		return true;
	}

	static bool consumeBodyBytes(const char *data, size_t length, StreamingResponseState &state, string &failReason)
	{
		if (length == 0)
			return true;
		if (state.chunkedResponse)
			return consumeChunkedBody(data, length, state, failReason);
		return consumeIdentityBody(data, length, state, failReason);
	}

	static bool processIncomingResponseData(TestCase &config, StreamingResponseState &state, const char *data, size_t length, string &failReason)
	{
		if (!state.headerComplete)
		{
			state.headerBuffer.append(data, length);
			size_t headerEnd = state.headerBuffer.find("\r\n\r\n");
			if (headerEnd == string::npos)
			{
				if (state.headerBuffer.size() > 65536)
				{
					failReason = "Response header is too large.";
					return false;
				}
				return true;
			}

			size_t bodyStart = headerEnd + 4;
			string bodyRemainder;
			if (bodyStart < state.headerBuffer.size())
				bodyRemainder = state.headerBuffer.substr(bodyStart);

			state.headerBuffer.erase(bodyStart);
			state.headerComplete = true;
			state.chunkedResponse = hasChunkedTransferEncoding(state.headerBuffer);
			state.hasContentLength = parseContentLengthHeader(state.headerBuffer, state.contentLength);

			config.headerLength = state.headerBuffer.size();
			config.response = state.headerBuffer;
			config.contentLength = state.hasContentLength ? state.contentLength : 0;

			if (!validateHeaderPatterns(state.headerBuffer, state.headerExpectedPatterns, failReason))
				return false;

			if (state.bodyPatternEnabled && state.hasContentLength && state.contentLength != state.bodyExpectedCount)
			{
				failReason = "Content-Length does not match expected repeated body size.";
				return false;
			}

			if (!state.chunkedResponse && state.hasContentLength && state.contentLength == 0)
				state.responseComplete = true;

			if (!bodyRemainder.empty())
				return consumeBodyBytes(bodyRemainder.c_str(), bodyRemainder.size(), state, failReason);
			return true;
		}

		return consumeBodyBytes(data, length, state, failReason);
	}

	static string buildStreamingBodySummary(const StreamingResponseState &state)
	{
		if (state.bodyPatternEnabled)
		{
			stringstream summary;
			summary << "'" << state.bodyExpectedChar << "' repeated " << state.bodyCount << " times";
			return summary.str();
		}
		if (state.decodedBodyBytes == 0)
			return "(empty)";
		return "body size " + to_string(state.decodedBodyBytes) + " bytes";
	}

	static string buildStreamingSendSplitProgress(size_t sentBytes, size_t headerPlannedBytes, size_t bodyPlannedBytes)
	{
		size_t sentHeaderBytes = min(sentBytes, headerPlannedBytes);
		size_t sentBodyBytes = 0;
		if (sentBytes > headerPlannedBytes)
			sentBodyBytes = sentBytes - headerPlannedBytes;

		return "Streaming request... (header " +
			to_string(sentHeaderBytes) + "/" + to_string(headerPlannedBytes) +
			" bytes, body " + to_string(sentBodyBytes) + "/" +
			to_string(bodyPlannedBytes) + " bytes)";
	}

	static bool onResponsePeerClosed(StreamingResponseState &state, string &failReason)
	{
		if (!state.headerComplete)
		{
			failReason = "Connection closed before response headers were complete.";
			return false;
		}

		if (state.chunkedResponse)
		{
			if (!state.responseComplete)
			{
				failReason = "Connection closed before receiving the final chunk.";
				return false;
			}
			return true;
		}

		if (state.hasContentLength && state.decodedBodyBytes != state.contentLength)
		{
			failReason = "Connection closed before receiving full Content-Length body.";
			return false;
		}

		state.responseComplete = true;
		return true;
	}

	static bool finalizeStreamingValidation(const StreamingResponseState &state, string &failReason)
	{
		if (!state.headerComplete)
		{
			failReason = "No complete response header received.";
			return false;
		}

		if (state.bodyPatternEnabled && state.bodyCount != state.bodyExpectedCount)
		{
			failReason = "Response body size does not match expected repeated-count pattern.";
			return false;
		}

		if (state.hasContentLength && state.decodedBodyBytes != state.contentLength)
		{
			failReason = "Decoded body length does not match Content-Length.";
			return false;
		}

		return true;
	}

	static char getStreamingBodyChar(const string &description)
	{
		size_t charStart = description.find("'");
		if (charStart != string::npos && charStart + 2 < description.size())
			return description[charStart + 1];
		return 'x';
	}

	static void initStreamingSendState(TestCase &config, StreamingSendState &sendState)
	{
		sendState.headerSize = config.request.size();
		sendState.totalSendSize = sendState.headerSize;
		sendState.hasGeneratedBody = config.bodyTotalSize > 0;
		sendState.payloadBytesPlanned = config.bodyTotalSize;

		if (config.chunkGenerated)
		{
			size_t totalChunkCount = config.chunksRemaining;
			size_t totalChunkedBytes = totalChunkCount * config.chunkedBodyStart.size() + config.chunkedBodyEnd.size();
			sendState.totalSendSize += totalChunkedBytes;
		}
		else if (sendState.hasGeneratedBody)
		{
			sendState.totalSendSize += config.bodyTotalSize;
			char bodyChar = getStreamingBodyChar(config.bodyDescription);
			size_t segmentSize = min(config.bodyTotalSize, (size_t)config.maxSend);
			sendState.bodySegment = string(segmentSize, bodyChar);
		}
	}

	static bool sendHeaderStep(TestCase &config, StreamingSendState &sendState, bool &progress, string &failReason)
	{
		if (sendState.headerSendComplete)
			return true;

		size_t remaining = sendState.headerSize - sendState.headerSentBytes;
		size_t toSend = (remaining > config.maxSend) ? config.maxSend : remaining;

		int sentBytes = 0;
		if (!sendSocketData(config.socketIO, config.request.c_str() + sendState.headerSentBytes, toSend, sentBytes))
		{
			if (shouldAcceptEarlyCloseDuringUpload(config))
			{
				sendState.headerSentBytes = sendState.headerSize;
				sendState.headerSendComplete = true;
				sendState.requestSendComplete = true;
				return true;
			}
			failReason = "Failed to send request headers.";
			return false;
		}

		if (sentBytes > 0)
		{
			progress = true;
			sendState.headerSentBytes += sentBytes;
			config.sendedBytes += sentBytes;
		}

		sendState.headerSendComplete = sendState.headerSentBytes >= sendState.headerSize;
		// sleep(1);
		return true;
	}

	static bool sendChunkedBodyStep(TestCase &config, bool &progress, string &failReason)
	{
		if (config.chunksRemaining == 0 && !config.sendingEndChunk && config.chunkedBodyEndSentBytes == 0)
			config.sendingEndChunk = true;

		if (config.chunksRemaining > 0 && !config.sendingEndChunk)
		{
			size_t remainingInChunk = config.chunkedBodyStart.size() - config.chunkedBodyStartSentBytes;
			size_t toSend = (remainingInChunk > config.maxSend) ? config.maxSend : remainingInChunk;

			int sentBytes = 0;
			if (!sendSocketData(config.socketIO,
							config.chunkedBodyStart.c_str() + config.chunkedBodyStartSentBytes,
							toSend,
							sentBytes))
			{
				if (shouldAcceptEarlyCloseDuringUpload(config))
				{
					config.chunksRemaining = 0;
					config.sendingEndChunk = true;
					config.chunkedBodyEndSentBytes = config.chunkedBodyEnd.size();
					return true;
				}
				failReason = "Failed to send chunked body segment.";
				return false;
			}

			if (sentBytes > 0)
			{
				progress = true;
				config.chunkedBodyStartSentBytes += sentBytes;
				config.sendedBytes += sentBytes;

				if (config.chunkedBodyStartSentBytes >= config.chunkedBodyStart.size())
				{
					config.chunkedBodyStartSentBytes = 0;
					if (config.chunksRemaining > 0)
						config.chunksRemaining--;
					if (config.chunksRemaining == 0)
						config.sendingEndChunk = true;
				}
			}
			return true;
		}

		if (config.sendingEndChunk && config.chunkedBodyEndSentBytes < config.chunkedBodyEnd.size())
		{
			size_t remainingEnd = config.chunkedBodyEnd.size() - config.chunkedBodyEndSentBytes;
			size_t toSend = (remainingEnd > config.maxSend) ? config.maxSend : remainingEnd;

			int sentBytes = 0;
			if (!sendSocketData(config.socketIO,
							config.chunkedBodyEnd.c_str() + config.chunkedBodyEndSentBytes,
							toSend,
							sentBytes))
			{
				if (shouldAcceptEarlyCloseDuringUpload(config))
				{
					config.chunkedBodyEndSentBytes = config.chunkedBodyEnd.size();
					return true;
				}
				failReason = "Failed to send chunked terminator.";
				return false;
			}

			if (sentBytes > 0)
			{
				progress = true;
				config.chunkedBodyEndSentBytes += sentBytes;
				config.sendedBytes += sentBytes;
			}
		}

		return true;
	}

	static bool sendGeneratedBodyStep(TestCase &config, StreamingSendState &sendState, bool &progress, string &failReason)
	{
		if (config.bodyGeneratedBytes >= config.bodyTotalSize)
			return true;

		size_t remainingBody = config.bodyTotalSize - config.bodyGeneratedBytes;
		size_t toSend = (remainingBody > config.maxSend) ? config.maxSend : remainingBody;
		if (toSend > sendState.bodySegment.size())
			toSend = sendState.bodySegment.size();

		int sentBytes = 0;
		if (!sendSocketData(config.socketIO, sendState.bodySegment.c_str(), toSend, sentBytes))
		{
			if (shouldAcceptEarlyCloseDuringUpload(config))
			{
				config.bodyGeneratedBytes = config.bodyTotalSize;
				return true;
			}
			failReason = "Failed to send generated body segment.";
			return false;
		}

		if (sentBytes > 0)
		{
			progress = true;
			config.bodyGeneratedBytes += sentBytes;
			config.sendedBytes += sentBytes;
		}

		return true;
	}

	static bool sendStreamingRequestStep(TestCase &config, StreamingSendState &sendState, bool &progress, string &failReason)
	{
		progress = false;
		if (sendState.requestSendComplete)
			return true;

		if (!sendState.headerSendComplete)
		{
			if (!sendHeaderStep(config, sendState, progress, failReason))
				return false;
			if (!sendState.headerSendComplete || progress)
				return true;
		}
		

		if (config.chunkGenerated)
		{
			if (!sendChunkedBodyStep(config, progress, failReason))
				return false;
			sendState.requestSendComplete =
				(config.chunksRemaining == 0 &&
				 config.sendingEndChunk &&
				 config.chunkedBodyEndSentBytes >= config.chunkedBodyEnd.size());
			return true;
		}

		if (sendState.hasGeneratedBody)
		{
			if (!sendGeneratedBodyStep(config, sendState, progress, failReason))
				return false;
			sendState.requestSendComplete = (config.bodyGeneratedBytes >= config.bodyTotalSize);
			return true;
		}

		sendState.requestSendComplete = true;
		return true;
	}

	static size_t parseStreamingBodyTotalSize(const string &description)
	{
		if (description.empty())
			return 0;

		size_t repPos = description.find(" repeated ");
		size_t timesPos = description.find(" times");
		if (repPos == string::npos || timesPos == string::npos || timesPos <= repPos + 9)
			return 0;

		string countStr = description.substr(repPos + 9, timesPos - (repPos + 9));
		return strtoull(countStr.c_str(), NULL, 10);
	}

	static void resetStreamingRuntimeState(TestCase &config)
	{
		config.sendedBytes = 0;
		config.chunkedBodyStartSentBytes = 0;
		config.chunkedBodyEndSentBytes = 0;
		config.chunksRemaining = 0;
		config.sendingEndChunk = false;
		config.chunkGenerated = false;
		config.response.clear();
		config.streamedResponseBodySummary.clear();
		config.streamedResponseBodyBytes = 0;
		config.streamedRequestPayloadSize = 0;
		config.streamedRequestHeaderSize = 0;
		config.streamedResponseHeaderSize = 0;
		config.headerLength = 0;
		config.contentLength = 0;
		config.passed = false;
		config.bodyGeneratedBytes = 0;
		config.bodyTotalSize = 0;
		config.bodyDescription.clear();

		if (!config.body.empty())
		{
			config.bodyDescription = config.body;
			config.bodyTotalSize = parseStreamingBodyTotalSize(config.body);
			config.isBodyGenerationComplete = false;
		}
	}

	static void setStreamingRequestSizes(TestCase &config)
	{
		string requestHeader;
		string requestBody;
		splitHttpMessage(config.request, requestHeader, requestBody);
		config.streamedRequestHeaderSize = requestHeader.size();
		config.streamedRequestPayloadSize =
			(config.bodyTotalSize > 0) ? config.bodyTotalSize : requestBody.size();
	}

	static bool readStreamingResponseData(TestCase &config,
									 StreamingResponseState &responseState,
									 string &failReason,
									 bool &responseComplete)
	{
		int receivedBytes = read(config.socketIO->GetFd(), config.responseBuffer, KBYTE);
		if (receivedBytes > 0)
		{
			if (!processIncomingResponseData(config,
								 responseState,
								 config.responseBuffer,
								 (size_t)receivedBytes,
								 failReason))
				return false;
			if (responseState.responseComplete)
				responseComplete = true;
			return true;
		}

		if (receivedBytes == 0)
		{
			if (!onResponsePeerClosed(responseState, failReason))
				return false;
			responseComplete = true;
			return true;
		}

		if (isWouldBlockError())
			return true;

		failReason = "Failed to receive response from the server.";
		return false;
	}

	static void setStreamingObservedResponse(TestCase &config,
									 const StreamingResponseState &responseState)
	{
		config.streamedResponseBodyBytes = responseState.decodedBodyBytes;
		config.streamedResponseHeaderSize = config.headerLength;
		config.streamedResponseBodySummary = buildStreamingBodySummary(responseState);
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

static string firstLineFromBuffer(const string &buffer)
{
	if (buffer.empty())
		return "";
	size_t lineEnd = buffer.find("\r\n");
	if (lineEnd == string::npos)
		lineEnd = buffer.find('\n');
	if (lineEnd == string::npos)
		return buffer;
	return buffer.substr(0, lineEnd);
}

static string collapseWhitespaceForLog(const string &value)
{
	string compact;
	compact.reserve(value.size());
	bool previousWasSpace = false;
	for (size_t i = 0; i < value.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(value[i]);
		if (isspace(c))
		{
			if (!previousWasSpace)
			{
				compact.push_back(' ');
				previousWasSpace = true;
			}
			continue;
		}
		compact.push_back(static_cast<char>(c));
		previousWasSpace = false;
	}
	return compact;
}

static string truncateForLog(const string &value, size_t maxLen)
{
	if (value.size() <= maxLen)
		return value;
	if (maxLen <= 3)
		return value.substr(0, maxLen);
	return value.substr(0, maxLen - 3) + "...";
}

static string expectedPatternsForLog(const vector<string> &patterns)
{
	string joined;
	for (size_t i = 0; i < patterns.size(); ++i)
	{
		if (!joined.empty())
			joined += " || ";
		joined += patterns[i];
	}
	return truncateForLog(collapseWhitespaceForLog(joined), 180);
}

void failChild(TestCase &config, string failedPattern)
{
	if (config.childIndex != -1)
	{
		int fail = -(config.childIndex + 1);
		write(config.pipeFd[1], &fail, sizeof(int));
		if (config.printForkFailureDetails)
		{
			string requestLine = truncateForLog(collapseWhitespaceForLog(firstLineFromBuffer(config.request)), 120);
			string statusLine = truncateForLog(collapseWhitespaceForLog(firstLineFromBuffer(config.response)), 120);
			string expected = expectedPatternsForLog(config.expectedResponse);

			ostringstream details;
			details << "Child " << config.childIndex << " failed: " << failedPattern;
			if (!requestLine.empty())
				details << " | request: " << requestLine;
			if (!expected.empty())
				details << " | expected: " << expected;
			if (!statusLine.empty())
				details << " | got: " << statusLine;
			else if (config.response.empty())
				details << " | got: <empty response>";

			if (config.detailPipeFd[1] != -1)
			{
				string detailMessage = details.str() + "\n";
				write(config.detailPipeFd[1], detailMessage.c_str(), detailMessage.size());
			}

			CLI::printError(details.str());
		}
		else
		{
			CLI::printError("Child failed, " + failedPattern);
		}
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

static char getRepeatedBodyChar(const string &description)
{
	size_t charStart = description.find("'");
	if (charStart != string::npos && charStart + 2 < description.size())
		return description[charStart + 1];
	return 'x';
}

void ATestList::CreateChunkedBody(TestCase &config)
{
	config.chunkGenerated = config.bodyDescription.find("chunked") != string::npos;
	if (!config.chunkGenerated)
		return;

	char bodyChar = getRepeatedBodyChar(config.bodyDescription);
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
	size_t totalSize = headerSize;
	bool hasGeneratedBody = config.bodyTotalSize > 0;
	char generatedBodyChar = getRepeatedBodyChar(config.bodyDescription);
	string bodySegment;
	if (config.chunkGenerated)
	{
		size_t totalChunkCount = config.chunksRemaining;
		size_t totalChunkedBytes = totalChunkCount * config.chunkedBodyStart.size() + config.chunkedBodyEnd.size();
		totalSize += totalChunkedBytes;
	}
	else if (hasGeneratedBody)
	{
		totalSize += config.bodyTotalSize;
		bodySegment = generateBodySegment(generatedBodyChar, min(config.bodyTotalSize, (size_t)config.maxSend));
	}
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
			size_t toSend = headerSize - config.sendedBytes;
			if (toSend > config.maxSend)
				toSend = config.maxSend;

			sentBytes = config.socketIO->Send((void *)(config.request.c_str() + config.sendedBytes), toSend);
			if (sentBytes <= 0)
			{
				failChild(config, "send header");
				CLI::printError("\nFailed to send request to the server.");
				return true;
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

			if (sentBytes <= 0)
			{
				break;
			}

			// Check if all chunked body sent
			if (config.chunksRemaining == 0 && config.sendingEndChunk &&
				config.chunkedBodyEndSentBytes >= config.chunkedBodyEnd.size())
			{
				break;
			}
		}
		else if (hasGeneratedBody)
		{
			// Phase 2: Send non-chunked body progressively using Content-Length.
			size_t remainingBody = config.bodyTotalSize - config.bodyGeneratedBytes;
			if (remainingBody == 0)
				break;

			size_t toSend = remainingBody;
			if (toSend > config.maxSend)
				toSend = config.maxSend;

			sentBytes = config.socketIO->Send((void *)bodySegment.c_str(), toSend);
			if (sentBytes <= 0)
			{
				failChild(config, "send generated body");
				CLI::printError("Failed to send generated body to the server.");
				return false;
			}

			config.bodyGeneratedBytes += sentBytes;
			config.sendedBytes += sentBytes;

			if (config.bodyGeneratedBytes >= config.bodyTotalSize)
				break;
		}
		else
		{
			// No chunked body, headers sent - done
			break;
		}

		if (config.isSubTest == false && config.printTest)
		{
			CLI::printHintProgress("Sending request to server... (" + to_string(config.sendedBytes) + "/" + to_string(totalSize) + " bytes sent)");
		}
		usleep(config.sleepTime);
	}
	if (config.isSubTest == false && config.printTest)
	{
		CLI::printHintProgress("Sending request to server... (" + to_string(config.sendedBytes) + "/" + to_string(totalSize) + " bytes sent)");
		cout << endl;
	}
	if (config.childIndex != -1 && config.isSubTest == false)
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

	string failedPattern;
	bool allMatched = validateExpectedPatternsAgainstResponse(config.response,
												responseBody,
												config.expectedResponse,
												failedPattern);

	if (!allMatched)
	{
		string followedLocation;
		if (followRedirectWithGet(config, followedLocation))
		{
			if (config.printTest && !config.isSubTest)
				CLI::printHint("Initial response was redirect, followed Location with GET: " + followedLocation);

			responseBody.clear();
			if (config.headerLength > 0 && config.headerLength <= config.response.size())
				responseBody = config.response.substr(config.headerLength);

			failedPattern.clear();
			allMatched = validateExpectedPatternsAgainstResponse(config.response,
													responseBody,
													config.expectedResponse,
													failedPattern);
		}
	}

	if (allMatched)
	{
		if (!config.isSubTest)
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
			config.passed = true;
		}
	}
	else
	{
		failChild(config, "Response did not match expected pattern: " + failedPattern);
		if (!config.isSubTest)
		{
			cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET;
			if (!failedPattern.empty())
				cerr << CLR_DIM << "  (missing: " << failedPattern << ")" << RESET;
			cerr << endl;
		}
	}

	if (!config.isSubTest && (config.printTest || config.passed == false))
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
	if (responseBody.empty() && !config.streamedResponseBodySummary.empty())
		responseBody = config.streamedResponseBodySummary;
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
			if (!config.streamedResponseBodySummary.empty())
				CLI::printWrapped(cout, string("Observed body: ") + config.streamedResponseBodySummary, CLR_DIM);
			if (config.streamedRequestHeaderSize > 0 || config.streamedRequestPayloadSize > 0)
				CLI::printWrapped(cout, string("Sent request sizes: header ") + to_string(config.streamedRequestHeaderSize) + " bytes, body " + to_string(config.streamedRequestPayloadSize) + " bytes", CLR_DIM);
			if (config.streamedResponseHeaderSize > 0 || config.streamedResponseBodyBytes > 0)
				CLI::printWrapped(cout, string("Observed response sizes: header ") + to_string(config.streamedResponseHeaderSize) + " bytes, body " + to_string(config.streamedResponseBodyBytes) + " bytes", CLR_DIM);
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
		if (!config.streamedResponseBodySummary.empty())
			CLI::printWrapped(cout, string("Observed body: ") + config.streamedResponseBodySummary, CLR_DIM);
		if (config.streamedRequestHeaderSize > 0 || config.streamedRequestPayloadSize > 0)
			CLI::printWrapped(cout, string("Sent request sizes: header ") + to_string(config.streamedRequestHeaderSize) + " bytes, body " + to_string(config.streamedRequestPayloadSize) + " bytes", CLR_DIM);
		if (config.streamedResponseHeaderSize > 0 || config.streamedResponseBodyBytes > 0)
			CLI::printWrapped(cout, string("Observed response sizes: header ") + to_string(config.streamedResponseHeaderSize) + " bytes, body " + to_string(config.streamedResponseBodyBytes) + " bytes", CLR_DIM);
		cout << CLI::midLine() << endl;
		cout << CLI::row(string(CLR_MENU_NUM) + "  1" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + "Print Tester Body" + RESET) << endl;
		cout << CLI::row(string(CLR_MENU_NUM) + "  2" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + "Print Server Body" + RESET) << endl;
		cout << CLI::row(string(CLR_MENU_NUM) + "  3" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + "Print configurationsForTestCase" + RESET) << endl;
		cout << CLI::row(string(CLR_MENU_NUM) + "  4" + RESET + CLR_DIM + "  " + SYM_RAQUO + " " + RESET + CLR_MENU_OPT + "Re-run Test" + RESET) << endl;
		cout << CLI::botLine() << endl;

		cout << CLR_PROMPT << "  " << SYM_ARROW << " Detail option (1-4, Enter to return): " << RESET;
		string choice = readInput();
		if (choice.empty() || (choice != "1" && choice != "2" && choice != "3" && choice != "4"))
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
			// if (toLowerCopy(requestHeader).find("transfer-encoding: chunked") != string::npos)
			// {
			// 	string decodedBody;
			// 	if (decodeChunkedBodyForDisplay(requestBody, decodedBody))
			// 		bodyToDisplay = decodedBody;
			// }
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
		else if (choice == "3")
		{
			cout << CLI::row(string(CLR_HEADER) + SYM_RAQUO + " configurationsForTestCase" + RESET) << endl;
			cout << CLI::midLine() << endl;
			CLI::printLines(cout, config.configurationsForTestCase.empty() ? string("(empty)") : config.configurationsForTestCase, CLR_DIM);
		}
		else if (choice == "4") {
			_reRunTest = true;
			return;
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
	do {
		_reRunTest = false;
	
	// Reset tracking variables for new test
	config.sendedBytes = 0;
	config.chunkedBodyStartSentBytes = 0;
	config.chunkedBodyEndSentBytes = 0;
	config.chunksRemaining = 0;
	config.sendingEndChunk = false;
	config.chunkGenerated = false;
	config.response.clear();
	config.streamedResponseBodySummary.clear();
	config.streamedResponseBodyBytes = 0;
	config.streamedRequestPayloadSize = 0;
	config.streamedRequestHeaderSize = 0;
	config.streamedResponseHeaderSize = 0;

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
	shutdown(config.socketIO->GetFd(), SHUT_WR);
	if (!ReadResponseFromServer(config))
	{
		multiplexer.DeleteFromEpoll(config.socketIO);
		return;
	}
	// assert
	actServerResponse(config);
	multiplexer.DeleteFromEpoll(config.socketIO);
	} while (_reRunTest);
}

void ATestList::RunStreamingTestCase(TestCase &config)
{
	do
	{
		_reRunTest = false;

		resetStreamingRuntimeState(config);
		setStreamingRequestSizes(config);

		if (config.printTest)
			printTestCard(config);
		if (config.printTest && !config.isSubTest)
			CLI::printHint("Streaming request sizes: header " + to_string(config.streamedRequestHeaderSize) + " bytes, body " + to_string(config.streamedRequestPayloadSize) + " bytes");

		if (!connectToServer(config))
			return;
		if (!setSocketNonBlocking(config.socketIO->GetFd()))
		{
			failChild(config, "set socket non-blocking");
			CLI::printError("Failed to enable non-blocking mode for streaming test.");
			multiplexer.DeleteFromEpoll(config.socketIO);
			return;
		}

		CreateChunkedBody(config);

		StreamingSendState sendState;
		initStreamingSendState(config, sendState);
		if (sendState.payloadBytesPlanned == 0)
			sendState.payloadBytesPlanned = config.streamedRequestPayloadSize;

		StreamingResponseState responseState;
		buildStreamingExpectations(config, responseState);

		if (!multiplexer.AddAsEpollOut(config.socketIO))
		{
			failChild(config, "add socket to epoll");
			CLI::printError("Failed to add socket to epoll.");
			return;
		}
		multiplexer.ChangeToEpollInOut(config.socketIO);

		bool requestHalfClosed = false;
		bool responseComplete = false;
		bool stopStreamingLoop = false;
		string failReason;

		while (!responseComplete && !stopStreamingLoop)
		{
			int eventCount = multiplexer.epollWait(config.timeout);
			if (eventCount == -1)
			{
				failReason = "epollWait -1";
				stopStreamingLoop = true;
				break;
			}
			if (eventCount == 0)
			{
				failReason = "epollWait timeout";
				stopStreamingLoop = true;
				break;
			}

			for (int i = 0; i < eventCount && !responseComplete && !stopStreamingLoop; ++i)
			{
				unsigned int events = multiplexer.eventList[i].events;

				if (events & EPOLLIN)
				{
					if (!readStreamingResponseData(config, responseState, failReason, responseComplete))
					{
						stopStreamingLoop = true;
						break;
					}
					if (responseComplete || responseState.responseComplete)
						continue;
				}

				if ((events & (EPOLLERR | EPOLLHUP)) && ((events & EPOLLIN) == 0))
				{
					if (!responseState.responseComplete)
					{
						if (!onResponsePeerClosed(responseState, failReason))
						{
							stopStreamingLoop = true;
							break;
						}
					}
					responseComplete = true;
					continue;
				}

				if ((events & EPOLLOUT) && !requestHalfClosed)
				{
					bool sendProgress = false;
					if (!sendStreamingRequestStep(config, sendState, sendProgress, failReason))
					{
						stopStreamingLoop = true;
						break;
					}
					if (sendProgress && config.isSubTest == false && config.printTest)
					{
						CLI::printHintProgress(buildStreamingSendSplitProgress(config.sendedBytes,
							sendState.headerSize,
							sendState.payloadBytesPlanned));
					}
					if (sendState.requestSendComplete && !sendState.writeShutdownSent)
					{
						shutdown(config.socketIO->GetFd(), SHUT_WR);
						if (!multiplexer.ChangeToEpollIn(config.socketIO))
						{
							failReason = "Failed to switch socket to response-read mode.";
							stopStreamingLoop = true;
							break;
						}
						sendState.writeShutdownSent = true;
						requestHalfClosed = true;
						if (config.printTest && !config.isSubTest)
							CLI::printHint("Request fully sent. Waiting for response...");
					}
				}
			}

			if (!failReason.empty())
			{
				stopStreamingLoop = true;
				break;
			}
			if (responseState.responseComplete)
			{
				responseComplete = true;
				break;
			}
		}

		bool passed = false;
		setStreamingObservedResponse(config, responseState);
		if (failReason.empty())
		{
			if (finalizeStreamingValidation(responseState, failReason))
				passed = true;
		}

		if (passed)
		{
			config.passed = true;
			if (!config.isSubTest)
			{
				_passedTests++;
				_failedTests--;
				if (config.printTest)
					cout << "  " << CLI::passBadge() << "  " << CLR_PASS << config.name << RESET << endl;
			}
		}
		else
		{
			failChild(config, failReason.empty() ? string("streaming validation failed") : failReason);
			config.passed = false;
			if (!config.isSubTest)
			{
				cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET;
				if (!failReason.empty())
					cerr << CLR_DIM << "  (reason: " << failReason << ")" << RESET;
				cerr << endl;
			}
		}

		if (config.isSubTest == false && config.printTest)
		{
			CLI::printHintProgress(buildStreamingSendSplitProgress(config.sendedBytes,
				sendState.headerSize,
				sendState.payloadBytesPlanned));
			cout << endl;
			CLI::printHint("Observed response sizes: header " + to_string(config.streamedResponseHeaderSize) + " bytes, body " + to_string(config.streamedResponseBodyBytes) + " bytes");
			if (!config.streamedResponseBodySummary.empty())
				CLI::printHint("Observed response body: " + config.streamedResponseBodySummary);
		}

		if (!config.isSubTest && (config.printTest || config.passed == false))
			printServerResponseHeader(config);

		multiplexer.DeleteFromEpoll(config.socketIO);
	} while (_reRunTest);
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
	if (pipe(config.detailPipeFd) == -1)
	{
		close(config.pipeFd[0]);
		close(config.pipeFd[1]);
		cerr << "  " << CLI::failBadge() << "  " << CLR_FAIL << config.name << RESET << " (detail pipe creation failed)" << endl;
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
			close(config.detailPipeFd[0]);
			close(config.detailPipeFd[1]);
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
			close(config.detailPipeFd[0]);
			return true;
		}
	}
	return true;
}

bool ATestList::runChildTestCase(TestCase &childConfig)
{
	childConfig.isSubTest = true;
	int childSuccessCount = 0;
	int pass = 1;
	// int fail = -(childConfig.childIndex + 1);
	isListOfTests = true;
	multiplexer.epoolInit();
	if (childConfig.pipeFd[0] != -1)
	{
		close(childConfig.pipeFd[0]);
		childConfig.pipeFd[0] = -1;
	}
	if (childConfig.detailPipeFd[0] != -1)
	{
		close(childConfig.detailPipeFd[0]);
		childConfig.detailPipeFd[0] = -1;
	}
	for (int j = 0; j < childConfig.totalRequests; ++j)
	{
		childConfig.sendedBytes = 0;
		childConfig.response.clear();
		childConfig.headerLength = 0;
		childConfig.contentLength = 0;

		if (childConfig.request.find("POST") != string::npos)
		{
			RunStreamingTestCase(childConfig);
		}
		else
		{
			RunTestCase(childConfig);
		}
		if (childConfig.passed)
		{
			write(childConfig.pipeFd[1], &pass, sizeof(int));
			childSuccessCount++;
		} else {
			break;
		}

		multiplexer.DeleteFromEpoll(childConfig.socketIO);
		delete childConfig.socketIO;
	}

	write(childConfig.pipeFd[1], &childSuccessCount, sizeof(int));
	close(childConfig.pipeFd[1]);
	if (childConfig.detailPipeFd[1] != -1)
		close(childConfig.detailPipeFd[1]);

	exit(childSuccessCount == childConfig.totalRequests ? 0 : 1);
}

void ATestList::getChildResults(TestCase &config)
{
	if (config.pipeFd[1] != -1)
		close(config.pipeFd[1]);
	if (config.detailPipeFd[1] != -1)
		close(config.detailPipeFd[1]);

	int totalSuccessCount = 0;
	bool anyChildFailed = false;
	int failedChildIndex = -1;
	string failureDetails;
	int totalExpected = config.forkCount * config.totalRequests;
	int percentStep = totalExpected / 100;
	if (percentStep == 0) percentStep = 1;
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
				if (totalSuccessCount % percentStep == 0) {
					CLI::printHintProgress("request pass test... (" + to_string(totalSuccessCount) + "/" + to_string(totalExpected) + " OK)");
				}
			}
		}
		else
		{
			anyChildFailed = true;
		}
	}
	cout << endl;
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

	if (config.printForkFailureDetails)
	{
		char detailBuffer[1024];
		while (true)
		{
			ssize_t bytesRead = read(config.detailPipeFd[0], detailBuffer, sizeof(detailBuffer));
			if (bytesRead > 0)
			{
				failureDetails.append(detailBuffer, static_cast<size_t>(bytesRead));
				continue;
			}
			if (bytesRead == -1 && errno == EINTR)
				continue;
			break;
		}
	}
	close(config.detailPipeFd[0]);

	if (anyChildFailed && config.printForkFailureDetails)
	{
		if (failureDetails.empty())
		{
			CLI::printError("Fork child failed but no detail message was captured.");
		}
		else
		{
			istringstream detailsStream(failureDetails);
			string line;
			while (getline(detailsStream, line))
			{
				if (!line.empty())
					CLI::printError(line);
			}
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
		printTestCard(config);
		getChildResults(config);
	}
}
