#pragma once

#include "UnitTests/Infrastructure/UnitTestInterface.h"

namespace UnitTests
{
	class UnitTestCoreContainers: public UnitTestInterface
	{
	public:
		UnitTestCoreContainers(const Dia::Core::Containers::String32& name): UnitTestInterface(name){}
		UnitTestCoreContainers(void) : UnitTestInterface(){};
		virtual ~UnitTestCoreContainers(void){};

		virtual UnitTestInterface::Types Type()const{return UnitTestInterface::kContainer;};
		const char* TypeName()const { return "Core::Containers";}
	};
}