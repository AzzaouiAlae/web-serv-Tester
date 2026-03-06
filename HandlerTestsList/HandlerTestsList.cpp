#include "HandlerTestsList.hpp"

HandlerTestsList::HandlerTestsList() 
{
	CreateTests();
}

void HandlerTestsList::CreateTests() {
	_happyPathTests = new HappyPathTests();
}

HandlerTestsList::~HandlerTestsList() {
	for (auto testList : ATestList::getTestLists()) {
		delete testList;
	}
}