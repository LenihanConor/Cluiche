#include "DiaCore/Strings/String8.h"

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
			DIA_TYPE_DEFINITION(String8)
				DIA_TYPE_ADD_VARIABLE("mData", mData)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, String8::Serialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, String8::Deserialize)
			DIA_TYPE_DEFINITION_END()

			void String8::Serialize(const Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, Json::Value& jsonData)
			{
				std::string str;
				unsigned int size = currentTypeVariable.GetNumberOfElements();
				for (unsigned int i = 0; i < size; i++)
				{
					// Set the meta data
					unsigned int hashID = currentTypeVariable.GetClassDefinition()->GetUniqueCRC();
					const char* name = currentTypeVariable.GetName();
					const char* className = currentTypeVariable.GetClassDefinition()->GetName();

					Json::Value newClassJsonData;

					newClassJsonData[Dia::Core::Types::TypeJsonSerializer::MetaData::GetMetaData(Dia::Core::Types::TypeJsonSerializer::MetaData::EFlagName::ClassName)] = className;
					newClassJsonData[Dia::Core::Types::TypeJsonSerializer::MetaData::GetMetaData(Dia::Core::Types::TypeJsonSerializer::MetaData::EFlagName::CRC)] = hashID;

					// Pull out the string
					DIA_ASSERT(currentTypeVariable.IsClassType(), "The string serializer did not find the expected class definition");

					const Dia::Core::Types::TypeDefinition::VariableLinkListNode* variable = currentTypeVariable.GetClassDefinition()->GetVariables().HeadConst();

					DIA_ASSERT(variable, "A variable type must be defined");

					const Dia::Core::Types::TypeVariable& typeVariable = *variable->GetPayloadConst();

					for (unsigned int j = 0; j < Capacity(); j++)
					{
						char temp = typeVariable.GetArithmeticValue<char>(instance, j);
						str.push_back(temp);

						if (temp == '\0')
						{
							break;
						}
					}

					str.shrink_to_fit();
					
					//Commit to the jsondata function
					if (currentTypeVariable.GetNumberOfElements() > 1)
						jsonData[name][i] = newClassJsonData;
					else
						jsonData[name] = newClassJsonData;
				}
				
				jsonData[currentTypeVariable.GetName()] = str;
			}

			void String8::Deserialize(Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, const Json::Value& jsonData)
			{
				std::string str = jsonData.asString();

				DIA_ASSERT(str.length() <= Capacity(), "String being read in will not fit in size of string being allocated");

				char* pointerToDst = reinterpret_cast<char*>(instance.Pointee());
				Dia::Core::StringCopy(pointerToDst, &str[0], str.length());
			}
		}
	}
}