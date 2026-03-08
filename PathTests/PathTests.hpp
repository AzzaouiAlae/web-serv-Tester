#pragma once
#include "../ATestList/ATestList.hpp"

class PathTests : public ATestList
{
	// 404 - Not Found
	void NotFoundFileTest();
	void NotFoundDirectoryTest();

	// 403 - Forbidden
	void ForbiddenFileTest();
	void ForbiddenDirectoryTest();
	void DirectoryWithoutIndexTest();

	// 400 - Malformed / Dangerous Paths
	void PathTraversalTest();
	void EncodedTraversalTest();
	void NullByteInPathTest();
	void DoubleSlashPathTest();

	// 414 - URI Too Long
	void VeryLongPathTest();
	
	public:
	void AddAllTests();
	PathTests();
	~PathTests();
};