#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestTimeServer: public UnitTestCore
	{
	public:
		UnitTestTimeServer(const Dia::Core::Containers::String32& name);
		UnitTestTimeServer(void);
		
		void DoTest();
	};
}