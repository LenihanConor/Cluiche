#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestTimer: public UnitTestCore
	{
	public:
		UnitTestTimer(const Dia::Core::Containers::String32& name);
		UnitTestTimer(void);
		
		void DoTest();
	};
}