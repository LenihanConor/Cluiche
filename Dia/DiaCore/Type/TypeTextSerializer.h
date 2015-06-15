#ifndef DIA_TYPE_TEXT_SERIALIZER_H
#define DIA_TYPE_TEXT_SERIALIZER_H

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
			//	TypeTextSerializer
			//------------------------------------------------------------------------------------
			class TypeTextSerializer
			{
			public:
				TypeTextSerializer();
				
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
			inline void TypeTextSerializer::Serialize(const T& object, Dia::Core::Containers::StringWriter& buffer)
			{
				Serialize( object.CreateTypeInstanceConst(), buffer );
			}

			//------------------------------------------------------------------------------------
			template<class T> 
			inline void TypeTextSerializer::Deserialize(T& object, Dia::Core::Containers::StringReader& buffer)
			{
				Deserialize( object.CreateTypeInstance(), buffer );
			}
		}
	}
}

#endif