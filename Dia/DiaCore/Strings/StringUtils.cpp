#include "DiaCore/Strings/stringutils.h"

#include <DiaCore/Core/Assert.h>
#include "DiaCore/Json/external/json/value.h"
#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeJsonSerializer.h"

namespace Dia
{
	namespace Core
	{
		//------------------------------------------------------------------------------------
		void StringDeserialize(Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, const Json::Value& jsonData, Dia::Core::Types::TypeJsonSerializerExternalDeserializeInterface& parent)
		{
			unsigned int size = 1;
			if (jsonData.isArray())
			{
				size = jsonData.size();

				DIA_ASSERT(currentTypeVariable.GetNumberOfElements() >= jsonData.size(), "Json array is outbounding the c++ array");
			}

			for (unsigned int i = 0; i < size; i++)
			{
				std::string str;
				if (jsonData.isArray())
				{
					str = jsonData[i].asString();
				}
				else
				{
					str = jsonData.asString();
				}

				char* pointeeAsType = reinterpret_cast<char*>(instance.Pointee());
				char* pointerToDst = reinterpret_cast<char*>(((currentTypeVariable.GetOffsetFromParent(i) / sizeof(char)) + pointeeAsType));

				Dia::Core::StringCopy(pointerToDst, &str[0], str.length());
			}
		}

		//------------------------------------------------------------------------------------
		void StringSerialize(const Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, Json::Value& jsonData, Dia::Core::Types::TypeJsonSerializerExternalSerializeInterface& parent)
		{
			unsigned int size = currentTypeVariable.GetNumberOfElements();
			for (unsigned int i = 0; i < size; i++)
			{
				unsigned int hashID = currentTypeVariable.GetClassDefinition()->GetUniqueCRC();
				const char* name = currentTypeVariable.GetName();
				const char* className = currentTypeVariable.GetClassDefinition()->GetName();

				Json::Value newClassJsonData;

				parent.AddCRCToCRCArray(className, hashID);

				newClassJsonData[Dia::Core::Types::TypeJsonSerializer::MetaData::GetMetaData(Dia::Core::Types::TypeJsonSerializer::MetaData::EFlagName::ClassName)] = className;

				Dia::Core::Types::TypeInstance newInstance(currentTypeVariable.GetClassDefinition(), currentTypeVariable.GetClassPointee(instance, i));

				parent.WriteVariables(newInstance, newClassJsonData);

				if (currentTypeVariable.GetNumberOfElements() > 1)
					jsonData[name][i] = newClassJsonData["mData"];
				else
					jsonData[name] = newClassJsonData["mData"];;

			}
		}
	}
}