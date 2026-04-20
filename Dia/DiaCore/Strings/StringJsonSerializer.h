#ifndef DIA_STRING_JSON_SERIALIZER_H
#define DIA_STRING_JSON_SERIALIZER_H

namespace Json { class Value; }

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeInstance;
			class TypeVariable;
			class TypeJsonSerializerExternalSerializeInterface;
			class TypeJsonSerializerExternalDeserializeInterface;
		}

		namespace Strings
		{
			void JsonSerializeString(const Types::TypeInstance& instance, const Types::TypeVariable& currentTypeVariable, Json::Value& jsonData, Types::TypeJsonSerializerExternalSerializeInterface& parent, unsigned int capacity);
			void JsonDeserializeString(Types::TypeInstance& instance, const Types::TypeVariable& currentTypeVariable, const Json::Value& jsonData, Types::TypeJsonSerializerExternalDeserializeInterface& parent, unsigned int capacity);
		}
	}
}

#endif // DIA_STRING_JSON_SERIALIZER_H
