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

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributesCustomJsonSerializer
			//---------------------------------------------------------------------------------------------------------

			const StripStringCRC TypeVariableAttributesCustomJsonSerializer::mAttributeID("TypeVariableAttributesCustomJsonSerializer");

			TypeVariableAttributesCustomJsonSerializer::TypeVariableAttributesCustomJsonSerializer(CustomSerializer func)
				: TypeVariableAttributes()
			{
				mFuncHandler = func;
			}

			void TypeVariableAttributesCustomJsonSerializer::Serialize(const TypeInstance& instance, const TypeVariable& currentTypeVariable, Json::Value& jsonData, TypeJsonSerializerExternalSerializeInterface& parent)const
			{
				DIA_ASSERT(mFuncHandler != nullptr, "No serialize function was set for TypeVariableAttributesCustomJsonSerializer on %s", instance.GetTypeDescriptor()->GetName());

				if (mFuncHandler)
				{
					mFuncHandler(instance,  currentTypeVariable, jsonData, parent);
				}
			}

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributesCustomJsonDeserializer
			//---------------------------------------------------------------------------------------------------------

			const StripStringCRC TypeVariableAttributesCustomJsonDeserializer::mAttributeID("TypeVariableAttributesCustomJsonDeserializer");

			TypeVariableAttributesCustomJsonDeserializer::TypeVariableAttributesCustomJsonDeserializer(CustomDeserializer func)
				: TypeVariableAttributes()
			{
				mFuncHandler = func;
			}

			void TypeVariableAttributesCustomJsonDeserializer::Deserialize(TypeInstance& instance, const TypeVariable& currentTypeVariable, const Json::Value& jsonData, TypeJsonSerializerExternalDeserializeInterface& parent)const
			{
				DIA_ASSERT(mFuncHandler != nullptr, "No serialize function was set for TypeVariableAttributesCustomJsonSerializer on %s", instance.GetTypeDescriptor()->GetName());

				if (mFuncHandler)
				{
					mFuncHandler(instance, currentTypeVariable, jsonData, parent);
				}
			}
		}
	}
}