#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestVector3D: public UnitTestMaths
	{
	public:
		UnitTestVector3D(const Dia::Core::Containers::String32& name);
		UnitTestVector3D(void);
		
		void DoTest();
	};
}