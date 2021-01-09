#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestAngle: public UnitTestMaths
	{
	public:
		UnitTestAngle(const Dia::Core::Containers::String32& name);
		UnitTestAngle(void);
		
		void DoTest();
	};
}