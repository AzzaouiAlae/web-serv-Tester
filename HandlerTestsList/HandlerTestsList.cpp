#include "HandlerTestsList.hpp"

HandlerTestsList::HandlerTestsList() 
{
	CreateTests();
}

void HandlerTestsList::CreateTests() {
	_happyPathTests = new HappyPathTests();
	_errorHandlingTests  = new ErrorHandlingTests();
	_clientBodySizeTests = new ClientBodySizeTests();
}

HandlerTestsList::~HandlerTestsList() {
	for (auto testList : ATestList::getTestLists()) {
		delete testList;
	}
}