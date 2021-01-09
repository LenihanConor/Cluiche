#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestTypeTraits: public UnitTestCore
	{
	public:
		UnitTestTypeTraits(const Dia::Core::Containers::String32& name);
		UnitTestTypeTraits(void);
		
		void DoTest();
	};
}