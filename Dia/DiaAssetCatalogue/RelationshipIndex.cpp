#include "DiaAssetCatalogue/RelationshipIndex.h"
#include "DiaAssetCatalogue/AssetRegistry.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// RelationshipIndex
		//------------------------------------------------------------------------------------
		RelationshipIndex::RelationshipIndex()
			: mReverseCacheDirty(true)
			, mReverseIndex()
		{}

		void RelationshipIndex::GetForwardRefs(const Dia::Core::StringCRC& assetId,
			const AssetRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& results) const
		{
			const AssetRecord* record = registry.FindById(assetId);
			if (record == nullptr)
			{
				return;
			}

			for (unsigned int i = 0; i < record->mReferences.Size(); ++i)
			{
				if (!results.IsFull())
				{
					results.Add(record->mReferences[i]);
				}
			}
		}

		void RelationshipIndex::GetForwardRefsByType(const Dia::Core::StringCRC& assetId,
			const Dia::Core::StringCRC& relationshipType,
			const AssetRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& results) const
		{
			const AssetRecord* record = registry.FindById(assetId);
			if (record == nullptr)
			{
				return;
			}

			for (unsigned int i = 0; i < record->mReferences.Size(); ++i)
			{
				if (record->mReferences[i].mRelationshipType == relationshipType)
				{
					if (!results.IsFull())
					{
						results.Add(record->mReferences[i].mTargetAssetId);
					}
				}
			}
		}

		void RelationshipIndex::GetReverseRefs(const Dia::Core::StringCRC& assetId,
			const AssetRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& results)
		{
			if (mReverseCacheDirty)
			{
				BuildReverseIndex(registry);
			}

			for (unsigned int i = 0; i < mReverseIndex.Size(); ++i)
			{
				const ReverseEntry& entry = mReverseIndex[i];
				if (entry.mTargetId == assetId)
				{
					if (!results.IsFull())
					{
						RelationshipEdge edge(entry.mRelType, entry.mFromId);
						results.Add(edge);
					}
				}
			}
		}

		void RelationshipIndex::GetReverseRefsByType(const Dia::Core::StringCRC& assetId,
			const Dia::Core::StringCRC& relationshipType,
			const AssetRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& results)
		{
			if (mReverseCacheDirty)
			{
				BuildReverseIndex(registry);
			}

			for (unsigned int i = 0; i < mReverseIndex.Size(); ++i)
			{
				const ReverseEntry& entry = mReverseIndex[i];
				if (entry.mTargetId == assetId && entry.mRelType == relationshipType)
				{
					if (!results.IsFull())
					{
						results.Add(entry.mFromId);
					}
				}
			}
		}

		void RelationshipIndex::InvalidateReverseCache()
		{
			mReverseCacheDirty = true;
		}

		void RelationshipIndex::BuildReverseIndex(const AssetRegistry& registry)
		{
			mReverseIndex.RemoveAll();

			unsigned int count = registry.GetCount();
			for (unsigned int i = 0; i < count; ++i)
			{
				// Access records by index via const ref
				const AssetRecord& record = registry.GetRecordByIndex(i);
				const Dia::Core::StringCRC& fromId = record.mId;

				for (unsigned int r = 0; r < record.mReferences.Size(); ++r)
				{
					if (!mReverseIndex.IsFull())
					{
						ReverseEntry entry;
						entry.mTargetId = record.mReferences[r].mTargetAssetId;
						entry.mFromId   = fromId;
						entry.mRelType  = record.mReferences[r].mRelationshipType;
						mReverseIndex.Add(entry);
					}
				}
			}

			mReverseCacheDirty = false;
		}

	} // namespace AssetCatalogue
} // namespace Dia
