#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestComponent: public UnitTestCoreContainers
	{
	public:
		UnitTestComponent(const Dia::Core::Containers::String32& name);
		UnitTestComponent(void);

		void DoTest();
	};
}