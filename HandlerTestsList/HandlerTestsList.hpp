#pragma once
#include "../Headers.hpp"
#include "HappyPathTests/HappyPathTests.hpp"

class HandlerTestsList {
	HappyPathTests *_happyPathTests;
public:
	HandlerTestsList();
	~HandlerTestsList();
	void CreateTests();
};
