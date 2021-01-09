#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestDynamicArray: public UnitTestCoreContainers
	{
	public:
		UnitTestDynamicArray(const Dia::Core::Containers::String32& name);
		UnitTestDynamicArray(void);

		void DoTest();
	};
}