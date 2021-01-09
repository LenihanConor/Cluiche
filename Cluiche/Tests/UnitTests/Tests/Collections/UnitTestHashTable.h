#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestHashTableC: public UnitTestCoreContainers
	{
	public:
		UnitTestHashTableC(const Dia::Core::Containers::String32& name);
		UnitTestHashTableC(void);
	
		void DoTest();
	};

	class UnitTestHashTable: public UnitTestCoreContainers
	{
	public:
		UnitTestHashTable(const Dia::Core::Containers::String32& name);
		UnitTestHashTable(void);

		void DoTest();
	};
}