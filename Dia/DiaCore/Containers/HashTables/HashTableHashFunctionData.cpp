#include "DiaCore/Containers/HashTables/HashTableHashFunctionData.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	HashTableHashFunctionData - Implementation
			//------------------------------------------------------------------------------------
			HashTableHashFunctionData::HashTableHashFunctionData()
				: mIsPopulated(false)
				, mTableSize(0)
			{}

			HashTableHashFunctionData::HashTableHashFunctionData(unsigned int size)
				: mIsPopulated(true)
				, mTableSize(size)
			{}

			void HashTableHashFunctionData::Populate(unsigned int size)
			{
				mIsPopulated  = true;
				mTableSize = size;
			}

			void HashTableHashFunctionData::DePopulate()
			{
				mIsPopulated  = false;
			}

			unsigned HashTableHashFunctionData::GetTableSize()const
			{
				DIA_ASSERT(mIsPopulated, "Table size not be created yet");

				return mTableSize;
			}
		}
	}
}