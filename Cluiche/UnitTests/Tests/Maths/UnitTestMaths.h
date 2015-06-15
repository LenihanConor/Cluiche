#pragma once

#include "UnitTests/Infrastructure/UnitTestInterface.h"

namespace UnitTests
{
	class UnitTestMaths: public UnitTestInterface
	{
	public:
		UnitTestMaths(const Dia::Core::Containers::String32& name): UnitTestInterface(name){}
		UnitTestMaths(void) : UnitTestInterface(){};
		virtual ~UnitTestMaths(void){};

		virtual UnitTestInterface::Types Type()const{return UnitTestInterface::kMaths;};
		const char* TypeName()const {return "Maths";}
	};
}