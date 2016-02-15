#include "DiaCore/Strings/String128.h"

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
			DIA_TYPE_DEFINITION(String128)
				DIA_TYPE_ADD_VARIABLE("mData", mData)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, String128::Serialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, String128::Deserialize)
			DIA_TYPE_DEFINITION_END()
		}
	}
}