#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestStringWriter: public UnitTestCoreContainers
	{
	public:
		UnitTestStringWriter(const Dia::Core::Containers::String32& name);
		UnitTestStringWriter(void);

		void DoTest();
	};
}