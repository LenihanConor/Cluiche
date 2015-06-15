#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestCircle2D: public UnitTestMaths
	{
	public:
		UnitTestCircle2D(const Dia::Core::Containers::String32& name);
		UnitTestCircle2D(void);
		
		void DoTest();
	};
}