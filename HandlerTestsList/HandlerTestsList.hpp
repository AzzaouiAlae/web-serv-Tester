#pragma once
#include "../Headers.hpp"
#include "HappyPathTests/HappyPathTests.hpp"
#include "ErrorHandlingTests/ErrorHandlingTests.hpp"
#include "ClientBodySizeTests/ClientBodySizeTests.hpp"
#include "MethodRestrictionTests/MethodRestrictionTests.hpp"
#include "RedirectionTests/RedirectionTests.hpp"
#include "AutoIndexTests/AutoIndexTests.hpp"
#include "RoutingTests/RoutingTests.hpp"
#include "CGITests/CGITests.hpp"
#include "ChunkedTransferTests/ChunkedTransferTests.hpp"
#include "SessionTests/SessionTests.hpp"
#include "StressTests/StressTests.hpp"
#include "../PathTests/PathTests.hpp"

class HandlerTestsList {
	HappyPathTests *_happyPathTests;
	ErrorHandlingTests *_errorHandlingTests;
	ClientBodySizeTests *_clientBodySizeTests;
	MethodRestrictionTests *_methodRestrictionTests;
	RedirectionTests *_redirectionTests;
	AutoIndexTests *_autoIndexTests;
	RoutingTests           *_routingTests;
	CGITests               *_cgiTests;
	ChunkedTransferTests   *_chunkedTransferTests;
	SessionTests           *_sessionTests;
	StressTests            *_stressTests;
	PathTests              *_pathTests;
public:
	HandlerTestsList();
	~HandlerTestsList();
	void CreateTests();
};
