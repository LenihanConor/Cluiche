#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestDynamicArrayC: public UnitTestCoreContainers
	{
	public:
		UnitTestDynamicArrayC(const Dia::Core::Containers::String32& name);
		UnitTestDynamicArrayC(void);

		void DoTest();
	};
}