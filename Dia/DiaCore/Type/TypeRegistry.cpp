#include "DiaCore/Type/TypeRegistry.h"

#include "DiaCore/Type/TypeDefinition.h"

#include <math.h>

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//------------------------------------------------------------------------------------
			//	TypeRegistry::TypeRegistryHashFunctor
			//------------------------------------------------------------------------------------
			unsigned int TypeRegistry::TypeRegistryHashFunctor::GetHashIndex(Key key, const TableData* tableData)const
			{
				static const unsigned int sTranslationToTableSpace = Dia::Core::CRC::MaxCRC() / TypeRegistry::kMaxTableSize;

				DIA_ASSERT(key.Value() != 0, "Cannot deal with 0 CRC");

				unsigned int index = static_cast<unsigned int>(floorf (static_cast<float>(key / sTranslationToTableSpace)));		
				return index;
			}


			//------------------------------------------------------------------------------------
			//	TypeRegistry
			//------------------------------------------------------------------------------------
			TypeRegistry::TypeRegistry()
				: mIDToTypeMap()
			{}
			
			void TypeRegistry::Add(TypeDefinition* type)
			{
				DIA_ASSERT(type, "Cannot be Null");
 				mIDToTypeMap.Add(type->GetUniqueCRC(), type );
			}

			void TypeRegistry::ClearAll()
			{
				mIDToTypeMap.RemoveAll();
			}
		}
	}
}