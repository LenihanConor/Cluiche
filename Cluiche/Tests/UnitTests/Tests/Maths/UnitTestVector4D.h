#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestVector4D: public UnitTestMaths
	{
	public:
		UnitTestVector4D(const Dia::Core::Containers::String32& name);
		UnitTestVector4D(void);
		
		void DoTest();
	};
}