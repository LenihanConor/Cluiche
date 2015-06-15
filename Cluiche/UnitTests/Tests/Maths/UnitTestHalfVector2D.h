#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestVectorHalf2D: public UnitTestMaths
	{
	public:
		UnitTestVectorHalf2D(const Dia::Core::Containers::String32& name);
		UnitTestVectorHalf2D(void);
		
		void DoTest();
	};
}