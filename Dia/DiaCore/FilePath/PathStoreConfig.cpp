#include "DiaCore/FilePath/PathStoreConfig.h"

#include "DiaCore/Type/TypeJsonSerializer.h"
#include "DiaCore/Json/external/json/value.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"
#include "DiaCore/Type/TypeJsonSerializer.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Strings/stringutils.h"

namespace Dia
{	
	namespace Core
	{
		DIA_TYPE_DEFINITION(AliasPathConfigTuple)
			DIA_TYPE_ADD_VARIABLE("mAlias", mAlias)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, Dia::Core::StringSerialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, Dia::Core::StringDeserialize)
			DIA_TYPE_ADD_VARIABLE("mPath", mPath)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, Dia::Core::StringSerialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, Dia::Core::StringDeserialize)
		DIA_TYPE_DEFINITION_END()

		DIA_TYPE_DEFINITION(AliasAppendPathConfig)
			DIA_TYPE_ADD_VARIABLE("mAlias", mAlias)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, Dia::Core::StringSerialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, Dia::Core::StringDeserialize)
			DIA_TYPE_ADD_VARIABLE("mBaseAlias", mBaseAlias)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, Dia::Core::StringSerialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, Dia::Core::StringDeserialize)
			DIA_TYPE_ADD_VARIABLE("mPathAppend", mPathAppend)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, Dia::Core::StringSerialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, Dia::Core::StringDeserialize)
		DIA_TYPE_DEFINITION_END()

		DIA_TYPE_DEFINITION(PathStoreConfig)
			DIA_TYPE_ADD_VARIABLE("mAliasPathTupleArray", mAliasPathTupleArray)
			DIA_TYPE_ADD_VARIABLE("mAliasAppendPathArray", mAliasAppendPathArray)
		DIA_TYPE_DEFINITION_END()
	}
}