#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestHalfFloat: public UnitTestMaths
	{
	public:
		UnitTestHalfFloat(const Dia::Core::Containers::String32& name);
		UnitTestHalfFloat(void);
		
		void DoTest();
	};
}