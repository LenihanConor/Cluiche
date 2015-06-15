#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestMatrix44: public UnitTestMaths
	{
	public:
		UnitTestMatrix44(const Dia::Core::Containers::String32& name);
		UnitTestMatrix44(void);
		
		void DoTest();
	};
}