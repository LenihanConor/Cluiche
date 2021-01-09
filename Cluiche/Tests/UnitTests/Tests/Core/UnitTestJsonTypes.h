#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

#include <DiaCore/Type/TypeDeclarationMacros.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>

namespace UnitTests
{
	class UnitTestJsonTypes: public UnitTestCore
	{
	public:
		UnitTestJsonTypes(const Dia::Core::Containers::String32& name);
		UnitTestJsonTypes(void);
		
		void DoTest();
	};
}