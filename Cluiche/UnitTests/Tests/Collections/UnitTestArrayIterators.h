#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestArrayIterators: public UnitTestCoreContainers
	{
	public:
		UnitTestArrayIterators(const Dia::Core::Containers::String32& name);
		UnitTestArrayIterators(void);

		void DoTest();
	};
}