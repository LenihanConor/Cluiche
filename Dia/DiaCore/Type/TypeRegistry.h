#ifndef DIA_TYPE_REGISTRY_H
#define DIA_TYPE_REGISTRY_H

#include "DiaCore/Containers/HashTables/HashTableC.h"
#include "DiaCore/CRC/CRC.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeDefinition;

			//------------------------------------------------------------------------------------
			//	Interface
			//------------------------------------------------------------------------------------
			class TypeRegistry
			{
			public:
				TypeRegistry();

				void Add(TypeDefinition* type);	
	
				void ClearAll();

			private:
				class TypeRegistryHashFunctor
				{
				public:
					typedef Dia::Core::CRC Key;
					typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

					unsigned int GetHashIndex(Key key, const TableData* tableData)const;
				};
				
				friend TypeRegistryHashFunctor;

				const static unsigned int kMaxTypes = 128; // Increment this as necessary
				const static unsigned int kMaxTableSize = static_cast<unsigned int>(kMaxTypes * 1.5); // Increment this as necessary

				typedef Containers::HashTableC <CRC, TypeDefinition*, TypeRegistryHashFunctor, kMaxTypes, kMaxTableSize> IDToTypeMap;	

 				IDToTypeMap mIDToTypeMap;
			};
		}
	}
}

#endif