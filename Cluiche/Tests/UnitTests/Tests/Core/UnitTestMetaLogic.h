#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestMetaLogic: public UnitTestCore
	{
	public:
		UnitTestMetaLogic(const Dia::Core::Containers::String32& name);
		UnitTestMetaLogic(void);
		
		void DoTest();
	};
}