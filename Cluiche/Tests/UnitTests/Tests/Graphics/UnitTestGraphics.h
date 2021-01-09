#pragma once

#include "UnitTests/Infrastructure/UnitTestInterface.h"

namespace UnitTests
{
	class UnitTestGraphics: public UnitTestInterface
	{
	public:
		UnitTestGraphics(const Dia::Core::Containers::String32& name) : UnitTestInterface(name){}
		UnitTestGraphics(void) : UnitTestInterface(){};
		virtual ~UnitTestGraphics(void){};

		virtual UnitTestInterface::Types Type()const{return UnitTestInterface::kGraphics;};
		const char* TypeName()const {return "Graphics";}
	};
}