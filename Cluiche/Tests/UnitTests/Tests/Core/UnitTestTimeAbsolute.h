#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestTimeAbsolute: public UnitTestCore
	{
	public:
		UnitTestTimeAbsolute(const Dia::Core::Containers::String32& name);
		UnitTestTimeAbsolute(void);
		
		void DoTest();
	};
}