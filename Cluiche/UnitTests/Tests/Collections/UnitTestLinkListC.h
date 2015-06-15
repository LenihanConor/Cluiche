#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestLinkLIstC: public UnitTestCoreContainers
	{
	public:
		UnitTestLinkLIstC(const Dia::Core::Containers::String32& name);
		UnitTestLinkLIstC(void);
	
		void DoTest();
	};
}