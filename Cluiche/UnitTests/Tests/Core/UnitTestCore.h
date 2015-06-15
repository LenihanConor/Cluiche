#pragma once

#include "UnitTests/Infrastructure/UnitTestInterface.h"

namespace UnitTests
{
	class UnitTestCore: public UnitTestInterface
	{
	public:
		UnitTestCore(const Dia::Core::Containers::String32& name): UnitTestInterface(name){}
		UnitTestCore(void) : UnitTestInterface(){};
		virtual ~UnitTestCore(void){};

		virtual UnitTestInterface::Types Type()const{return UnitTestInterface::kCore;};
		const char* TypeName()const {return "Core";}
	};
}