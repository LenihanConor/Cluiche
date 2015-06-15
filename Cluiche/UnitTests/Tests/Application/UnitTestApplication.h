#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestApplication: public UnitTestCore
	{
	public:
		UnitTestApplication(const Dia::Core::Containers::String32& name);
		UnitTestApplication(void);
		
		void DoTest();
	};
}