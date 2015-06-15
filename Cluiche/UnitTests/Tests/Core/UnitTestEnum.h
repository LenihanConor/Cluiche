#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestEnum: public UnitTestCore
	{
	public:
		UnitTestEnum(const Dia::Core::Containers::String32& name);
		UnitTestEnum(void);
		
		void DoTest();
	};
}