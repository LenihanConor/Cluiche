#include "DiaAssetCatalogue/AssetRegistry.h"

#include "DiaCore/CRC/CRC.h"

#include <math.h>
#include <string.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// AssetRecordHashFunctor
		//------------------------------------------------------------------------------------
		unsigned int AssetRegistry::AssetRecordHashFunctor::GetHashIndex(const Key& key, const TableData* tableData) const
		{
			static const unsigned int sTranslationToTableSpace = Dia::Core::CRC::MaxCRC() / AssetRegistry::kMaxTableSize;

			DIA_ASSERT(key.Value() != 0, "Cannot hash a zero CRC key");

			unsigned int index = static_cast<unsigned int>(floorf(static_cast<float>(key.Value()) / static_cast<float>(sTranslationToTableSpace)));
			return index;
		}

		//------------------------------------------------------------------------------------
		// IsValidAssetId
		//
		// Returns true if the StringCRC's string representation satisfies the "type.name" rules:
		//   - Exactly one '.' separator
		//   - Both parts non-empty
		//   - All characters lowercase alphanumeric, '_', or '-'
		//------------------------------------------------------------------------------------
		/*static*/ bool AssetRegistry::IsValidAssetId(const Dia::Core::StringCRC& id)
		{
			const char* str = id.AsChar();
			if (str == nullptr || str[0] == '\0')
			{
				return false;
			}

			int dotCount   = 0;
			int dotPos     = -1;
			int len        = 0;

			for (int i = 0; str[i] != '\0'; ++i)
			{
				char c = str[i];

				if (c == '.')
				{
					++dotCount;
					dotPos = i;
				}
				else if (c >= 'a' && c <= 'z')
				{
					// valid
				}
				else if (c >= '0' && c <= '9')
				{
					// valid
				}
				else if (c == '_' || c == '-')
				{
					// valid
				}
				else
				{
					return false; // invalid character
				}
				++len;
			}

			// Must have exactly one dot
			if (dotCount != 1)
			{
				return false;
			}

			// Both type and name parts must be non-empty
			if (dotPos == 0 || dotPos == len - 1)
			{
				return false;
			}

			return true;
		}

		//------------------------------------------------------------------------------------
		// AssetRegistry
		//------------------------------------------------------------------------------------
		AssetRegistry::AssetRegistry()
			: mRecordMap()
			, mCount(0)
			, mRelationshipIndex()
		{}

		AssetRegistry::AssetRegistry(const AssetRegistry& other)
			: mRecordMap()
			, mCount(0)
			, mRelationshipIndex()
		{
			for (unsigned int i = 0; i < other.mCount; ++i)
				Register(other.mRecordMap.GetItemByIndexConst(i));
		}

		AssetRegistry& AssetRegistry::operator=(const AssetRegistry& other)
		{
			if (this != &other)
			{
				mRecordMap.RemoveAll();
				mCount = 0;
				mRelationshipIndex = RelationshipIndex();
				for (unsigned int i = 0; i < other.mCount; ++i)
					Register(other.mRecordMap.GetItemByIndexConst(i));
			}
			return *this;
		}

		bool AssetRegistry::Register(const AssetRecord& record)
		{
			// Validate ID format
			if (!IsValidAssetId(record.mId))
			{
				return false;
			}

			// Reject duplicates
			if (mRecordMap.ContainsKey(record.mId))
			{
				return false;
			}

			mRecordMap.Add(record.mId, record);
			++mCount;

			// Invalidate reverse relationship cache
			mRelationshipIndex.InvalidateReverseCache();

			return true;
		}

		bool AssetRegistry::Remove(const Dia::Core::StringCRC& id)
		{
			if (!mRecordMap.ContainsKey(id))
			{
				return false;
			}

			mRecordMap.Remove(id);
			--mCount;

			// Invalidate reverse relationship cache
			mRelationshipIndex.InvalidateReverseCache();

			return true;
		}

		const AssetRecord* AssetRegistry::FindById(const Dia::Core::StringCRC& id) const
		{
			return mRecordMap.TryGetItemConst(id);
		}

		AssetRecord* AssetRegistry::FindById(const Dia::Core::StringCRC& id)
		{
			return mRecordMap.TryGetItem(id);
		}

		void AssetRegistry::QueryByType(const Dia::Core::StringCRC& typeId,
			Dia::Core::Containers::DynamicArrayC<const AssetRecord*, 64>& results) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				const AssetRecord& record = mRecordMap.GetItemByIndexConst(i);
				if (record.mAssetTypeId == typeId)
				{
					if (!results.IsFull())
					{
						results.Add(&record);
					}
				}
			}
		}

		void AssetRegistry::QueryByTag(const Dia::Core::StringCRC& tag,
			Dia::Core::Containers::DynamicArrayC<const AssetRecord*, 64>& results) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				const AssetRecord& record = mRecordMap.GetItemByIndexConst(i);
				for (unsigned int t = 0; t < record.mTags.Size(); ++t)
				{
					if (record.mTags[t] == tag)
					{
						if (!results.IsFull())
						{
							results.Add(&record);
						}
						break;
					}
				}
			}
		}

		unsigned int AssetRegistry::GetCount() const
		{
			return mCount;
		}

		const AssetRecord& AssetRegistry::GetRecordByIndex(unsigned int index) const
		{
			return mRecordMap.GetItemByIndexConst(index);
		}

		RelationshipIndex& AssetRegistry::GetRelationshipIndex()
		{
			return mRelationshipIndex;
		}

		const RelationshipIndex& AssetRegistry::GetRelationshipIndex() const
		{
			return mRelationshipIndex;
		}

	} // namespace AssetCatalogue
} // namespace Dia
