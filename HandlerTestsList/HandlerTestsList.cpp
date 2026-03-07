#include "HandlerTestsList.hpp"

HandlerTestsList::HandlerTestsList() 
{
	CreateTests();
}

void HandlerTestsList::CreateTests() {
	_happyPathTests = new HappyPathTests();
	_errorHandlingTests  = new ErrorHandlingTests();
	_clientBodySizeTests = new ClientBodySizeTests();
	_methodRestrictionTests = new MethodRestrictionTests();
	_redirectionTests = new RedirectionTests();
	_autoIndexTests = new AutoIndexTests();
	_routingTests           = new RoutingTests();
	_cgiTests               = new CGITests();
	_chunkedTransferTests   = new ChunkedTransferTests();
	_sessionTests           = new SessionTests();
	_stressTests            = new StressTests();
}

HandlerTestsList::~HandlerTestsList() {
	for (auto testList : ATestList::getTestLists()) {
		delete testList;
	}
}