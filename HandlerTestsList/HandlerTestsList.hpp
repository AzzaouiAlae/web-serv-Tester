#pragma once
#include "../Headers.hpp"
#include "HappyPathTests/HappyPathTests.hpp"
#include "ErrorHandlingTests/ErrorHandlingTests.hpp"
#include "ClientBodySizeTests/ClientBodySizeTests.hpp"

class HandlerTestsList {
	HappyPathTests *_happyPathTests;
	ErrorHandlingTests *_errorHandlingTests;
	ClientBodySizeTests *_clientBodySizeTests;
public:
	HandlerTestsList();
	~HandlerTestsList();
	void CreateTests();
};
