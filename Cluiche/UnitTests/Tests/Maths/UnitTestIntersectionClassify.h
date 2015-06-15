#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestIntersectionClassify: public UnitTestMaths
	{
	public:
		UnitTestIntersectionClassify(const Dia::Core::Containers::String32& name);
		UnitTestIntersectionClassify(void);
		
		void DoTest();
	};
}