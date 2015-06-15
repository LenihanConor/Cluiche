#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestCoreMaths: public UnitTestMaths
	{
	public:
		UnitTestCoreMaths(const Dia::Core::Containers::String32& name);
		UnitTestCoreMaths(void);
		
		void DoTest();
	};
}