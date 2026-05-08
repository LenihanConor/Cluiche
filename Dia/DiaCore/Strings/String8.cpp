#include "DiaCore/Strings/String8.h"

#include "DiaCore/Strings/StringUtils.h"
#include "DiaCore/Strings/StringJsonSerializer.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"
#include "DiaCore/Type/TypeJsonSerializer.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			DIA_TYPE_DEFINITION(String8)
				DIA_TYPE_ADD_VARIABLE("mData", mData)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, [](const Dia::Core::Types::TypeInstance& inst, const Dia::Core::Types::TypeVariable& var, Json::Value& json, Dia::Core::Types::TypeJsonSerializerExternalSerializeInterface& p){ Dia::Core::Strings::JsonSerializeString(inst, var, json, p, 8); })
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, [](Dia::Core::Types::TypeInstance& inst, const Dia::Core::Types::TypeVariable& var, const Json::Value& json, Dia::Core::Types::TypeJsonSerializerExternalDeserializeInterface& p){ Dia::Core::Strings::JsonDeserializeString(inst, var, json, p, 8); })
			DIA_TYPE_DEFINITION_END()
		}
	}
}
