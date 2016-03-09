#ifndef DIA_TYPE_JSON_SERIALIZER_H
#define DIA_TYPE_JSON_SERIALIZER_H

#include "DiaCore/Core/EnumClass.h"

namespace Json
{
	class Value;
}

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class StringWriter;
			class StringReader;
		};

		namespace Types
		{
			class TypeDefinition;
			class TypeInstance;
			class TypeRegistry;
			
			//------------------------------------------------------------------------------------
			//	TypeJsonSerializerExternalSerializeInterface
			//------------------------------------------------------------------------------------
			class TypeJsonSerializerExternalDeserializeInterface
			{
			public:
				virtual void ReadVariable(TypeInstance& instance, const Json::Value& data) = 0;
			};

			//------------------------------------------------------------------------------------
			//	TypeJsonSerializerExternalSerializeInterface
			//------------------------------------------------------------------------------------
			class TypeJsonSerializerExternalSerializeInterface
			{
			public:
				virtual void AddCRCToCRCArray(const char* name, unsigned int hashID) = 0;

				virtual void WriteVariables(const TypeInstance& instance, Json::Value& jsonData) = 0;
			};

			//------------------------------------------------------------------------------------
			//	TypeJsonSerializer
			//------------------------------------------------------------------------------------
			class TypeJsonSerializer
			{
			public:
				//------------------------------------------------------------------------------------
				//	MetaData
				//------------------------------------------------------------------------------------
				class MetaData
				{
				public:
					static const char sMetaDataPrefix = '_';

					CLASSEDENUM(EFlagName, \
						CE_ITEMVAL(Unknown, -1)\
						CE_ITEMVAL(ClassName, 0)\
						CE_ITEM(CRCArray), \
						Unknown \
						);

					static const char* GetMetaData(EFlagName flag)
					{
						return sMetaDataArray[flag];
					}

				private:
					static const char* sMetaDataArray[EFlagName::NumberOfItems];
				};

				TypeJsonSerializer();
				
				void Initilize(const TypeRegistry* registry);

 				template<class T> 
 				void Serialize(const T& object, Dia::Core::Containers::StringWriter& buffer);
				void Serialize(const TypeInstance& instance, Dia::Core::Containers::StringWriter& buffer);
				
			
				template<class T> 
				void Deserialize(T& object, Dia::Core::Containers::StringReader& buffer);
				void Deserialize(TypeInstance& instance, Dia::Core::Containers::StringReader& buffer);
	
			private:				
				const TypeRegistry* mRegistry;
			};

			//------------------------------------------------------------------------------------
			template<class T> 
			inline void TypeJsonSerializer::Serialize(const T& object, Dia::Core::Containers::StringWriter& buffer)
			{
				Serialize( object.CreateTypeInstanceConst(), buffer );
			}

			//------------------------------------------------------------------------------------
			template<class T> 
			inline void TypeJsonSerializer::Deserialize(T& object, Dia::Core::Containers::StringReader& buffer)
			{
				Deserialize( object.CreateTypeInstance(), buffer );
			}
		}
	}
}

#endif