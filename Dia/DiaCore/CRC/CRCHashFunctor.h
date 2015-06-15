#ifndef DIA_CRC_HASH_FUNCTOR
#define DIA_CRC_HASH_FUNCTOR

#include "DiaCore/CRC/CRC.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/CRC/StripStringCRC.h"

#include <DiaCore/Containers/HashTables/HashTableHashFunctionData.h>

namespace Dia
{
	namespace Core
	{
		class CRCHashFunctor
		{
		public:
			typedef Dia::Core::CRC Key;
			typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

			unsigned int GetHashIndex(Key& key, const TableData* tableData)const
			{
				unsigned int maxCRC = CRC::MaxCRC();
				if (key.Value() == 0)
				{
					return 0;
				}

				unsigned int hashIndex = key.Value() / maxCRC * 10;

				return hashIndex; 
			}
		};

		class StringCRCHashFunctor
		{
		public:
			typedef Dia::Core::StringCRC Key;
			typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

			unsigned int GetHashIndex(const Key& key, const TableData* tableData)const
			{
				unsigned int maxCRC = CRC::MaxCRC();
				if (key.Value() == 0)
				{
					return 0;
				}

				unsigned int hashIndex = key.Value() / maxCRC * 10;

				return hashIndex; 
			}
		};

		class StripStringCRCHashFunctor
		{
		public:
			typedef Dia::Core::StripStringCRC Key;
			typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

			unsigned int GetHashIndex(Key& key, const TableData* tableData)const
			{
				unsigned int maxCRC = CRC::MaxCRC();
				if (key.Value() == 0)
				{
					return 0;
				}

				unsigned int hashIndex = key.Value() / maxCRC * 10;

				return hashIndex; 
			}
		};
	}
}

#endif // DIA_ASSERT