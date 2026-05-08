#include "DiaCore/Strings/StringJsonSerializer.h"

#include "DiaCore/Json/external/json/json.h"
#include "DiaCore/Strings/StringUtils.h"
#include "DiaCore/Type/TypeInstance.h"
#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Type/TypeJsonSerializer.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Strings
		{
			void JsonDeserializeString(Types::TypeInstance& instance, const Types::TypeVariable& currentTypeVariable, const Json::Value& jsonData, Types::TypeJsonSerializerExternalDeserializeInterface& parent, unsigned int capacity)
			{
				std::string str = jsonData.asString();

				DIA_ASSERT(str.length() <= capacity, "String being read in will not fit in size of string being allocated");

				char* pointerToDst = reinterpret_cast<char*>(instance.Pointee());
				Dia::Core::StringCopy(pointerToDst, &str[0], static_cast<unsigned int>(str.length()));
			}

			void JsonSerializeString(const Types::TypeInstance& instance, const Types::TypeVariable& currentTypeVariable, Json::Value& jsonData, Types::TypeJsonSerializerExternalSerializeInterface& parent, unsigned int capacity)
			{
				unsigned int numElements = currentTypeVariable.GetNumberOfElements();
				for (unsigned int i = 0; i < numElements; i++)
				{
					const char* name = currentTypeVariable.GetName();

					DIA_ASSERT(currentTypeVariable.IsClassType(), "The string serializer did not find the expected class definition");

					const Dia::Core::Types::TypeDefinition::VariableLinkListNode* variable = currentTypeVariable.GetClassDefinition()->GetVariables().HeadConst();

					DIA_ASSERT(variable, "A variable type must be defined");

					const Dia::Core::Types::TypeVariable& typeVariable = *variable->GetPayloadConst();

					std::string str;
					for (unsigned int j = 0; j < capacity; j++)
					{
						char temp = typeVariable.GetArithmeticValue<char>(instance, j);
						str.push_back(temp);

						if (temp == '\0')
						{
							break;
						}
					}

					str.shrink_to_fit();

					if (numElements > 1)
						jsonData[name][i] = str;
					else
						jsonData[name] = str;
				}
			}
		}
	}
}
