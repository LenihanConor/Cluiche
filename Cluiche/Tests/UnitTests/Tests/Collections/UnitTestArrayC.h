#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestArrayC: public UnitTestCoreContainers
	{
	public:
		UnitTestArrayC(const Dia::Core::Containers::String32& name);
		UnitTestArrayC(void);
	
		void DoTest();
	};
}