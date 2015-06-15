#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestCircularBuffer: public UnitTestCoreContainers
	{
	public:
		UnitTestCircularBuffer(const Dia::Core::Containers::String32& name);
		UnitTestCircularBuffer(void);
	
		void DoTest();
	};
}