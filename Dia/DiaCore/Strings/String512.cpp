#include "DiaCore/Strings/String512.h"

#include "DiaCore/Strings/StringUtils.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"
#include "DiaCore/Type/TypeJsonSerializer.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"
#include "DiaCore/Json/external/json/value.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			DIA_TYPE_DEFINITION(String512)
				DIA_TYPE_ADD_VARIABLE("mData", mData)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, String512::Serialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, String512::Deserialize)
			DIA_TYPE_DEFINITION_END()
		}
	}
}