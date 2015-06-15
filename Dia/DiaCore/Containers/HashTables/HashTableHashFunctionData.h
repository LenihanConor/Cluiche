
#ifndef HASH_TABLE_HASH_FUNCTION_DATA
#define HASH_TABLE_HASH_FUNCTION_DATA

#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	HashTableHashFunctionData - Interface
			//------------------------------------------------------------------------------------
			class HashTableHashFunctionData
			{
			public:
				HashTableHashFunctionData();
				HashTableHashFunctionData(unsigned int size);

				void Populate(unsigned int size);
				void DePopulate();
				
				unsigned GetTableSize()const;

			private:
				bool			mIsPopulated;
				unsigned int	mTableSize;
			};
		}
	}
}
#endif