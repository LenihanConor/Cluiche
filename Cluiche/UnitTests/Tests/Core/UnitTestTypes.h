#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

#include <DiaCore/Type/TypeDeclarationMacros.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>

namespace UnitTests
{
	class UnitTestTypes: public UnitTestCore
	{
	public:
		UnitTestTypes(const Dia::Core::Containers::String32& name);
		UnitTestTypes(void);
		
		void DoTest();
	};
}