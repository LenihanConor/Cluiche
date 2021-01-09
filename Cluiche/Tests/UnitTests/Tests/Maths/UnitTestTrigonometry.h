#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestTrigonometry: public UnitTestMaths
	{
	public:
		UnitTestTrigonometry(const Dia::Core::Containers::String32& name);
		UnitTestTrigonometry(void);
		
		void DoTest();
	};
}