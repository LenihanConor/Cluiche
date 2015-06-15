#include "DiaCore/Type/TypeVariableAttributes.h"

#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Core/Log.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributesPointerAsObject
			//---------------------------------------------------------------------------------------------------------
			
			const StripStringCRC TypeVariableAttributesPointerAsObject::mAttributeID("TypeVariableAttributesPointerAsObject");

			TypeVariableAttributesPointerAsObject::TypeVariableAttributesPointerAsObject()
				: TypeVariableAttributes()
			{}

			void TypeVariableAttributesPointerAsObject::AssignedTo(const TypeVariable& typeVariable)
			{
				DIA_ASSERT(typeVariable.IsPointerType(), "Can only assign this attribute to pointers");
			}
		}
	}
}