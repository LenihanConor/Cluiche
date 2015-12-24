#ifndef DIA_TYPE_JSON_SERIALIZER_H
#define DIA_TYPE_JSON_SERIALIZER_H

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
			//	TypeJsonSerializer
			//------------------------------------------------------------------------------------
			class TypeJsonSerializer
			{
			public:
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