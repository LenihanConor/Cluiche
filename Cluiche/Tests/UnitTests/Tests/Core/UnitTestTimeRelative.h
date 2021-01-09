#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestTimeRelative: public UnitTestCore
	{
	public:
		UnitTestTimeRelative(const Dia::Core::Containers::String32& name);
		UnitTestTimeRelative(void);
		
		void DoTest();
	};
}