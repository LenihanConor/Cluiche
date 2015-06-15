#include "DiaCore/Type/TypeParameterInput.h"

#include "DiaCore/Type/TypeDefinition.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			void TypeParameterInput::SetBaseType(const TypeDefinition* baseType)
			{
				mBaseType = baseType;
			}
		}
	}
}