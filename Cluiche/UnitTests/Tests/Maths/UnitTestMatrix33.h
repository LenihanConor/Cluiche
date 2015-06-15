#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestMatrix33: public UnitTestMaths
	{
	public:
		UnitTestMatrix33(const Dia::Core::Containers::String32& name);
		UnitTestMatrix33(void);
		
		void DoTest();
	};
}