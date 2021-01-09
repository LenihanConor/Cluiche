#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestVectorUtil: public UnitTestMaths
	{
	public:
		UnitTestVectorUtil(const Dia::Core::Containers::String32& name);
		UnitTestVectorUtil(void);
		
		void DoTest();
	};
}