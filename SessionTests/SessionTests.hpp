#pragma once
#include "../ATestList/ATestList.hpp"

class SessionTests : public ATestList
{
	void SetCookieOnFirstVisitTest();
	void CookieValueNotEmptyTest();
	void HttpCookieEnvVarTest();
	void InvalidSessionNewCookieTest();
	void SessionDataStoredTest();
	void AddAllTests();

	// Extracts the value of a named cookie from a Set-Cookie response header.
	// Returns an empty string if the header or cookie name is not found.
	// Format searched: "Set-Cookie: <name>=<value>[; ...]"
	static string extractCookieValue(const string &response, const string &cookieName);
public:
	SessionTests();
	~SessionTests();
};
