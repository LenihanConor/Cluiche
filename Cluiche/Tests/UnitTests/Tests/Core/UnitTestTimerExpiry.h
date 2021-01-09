#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestTimerExpiry: public UnitTestCore
	{
	public:
		UnitTestTimerExpiry(const Dia::Core::Containers::String32& name);
		UnitTestTimerExpiry(void);
		
		void DoTest();
	};
}