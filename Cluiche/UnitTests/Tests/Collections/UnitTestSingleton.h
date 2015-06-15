#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestSingleton: public UnitTestCoreContainers
	{
	public:
		UnitTestSingleton(const Dia::Core::Containers::String32& name);
		UnitTestSingleton(void);

		void DoTest();
	};
}