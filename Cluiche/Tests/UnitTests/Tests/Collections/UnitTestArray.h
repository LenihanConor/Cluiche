#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestArray: public UnitTestCoreContainers
	{
	public:
		UnitTestArray(const Dia::Core::Containers::String32& name);
		UnitTestArray(void);
	
		void DoTest();
	};
}