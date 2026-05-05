#include "DiaAssetCatalogueEditor/Commands/DeleteRecordCommand.h"
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			DeleteRecordCommand::DeleteRecordCommand(
				Dia::AssetCatalogue::AssetRegistry& registry,
				Dia::AssetCatalogue::RelationshipIndex& relationships,
				const Dia::Core::StringCRC& recordId)
				: mRegistry(registry)
				, mRelationships(relationships)
				, mRecordId(recordId)
				, mDeletedRecord()
				, mDeletedEdges()
				, mExecuted(false)
			{
				// Snapshot the record and its forward refs before first Execute
				const AssetRecord* rec = mRegistry.FindById(mRecordId);
				if (rec)
				{
					mDeletedRecord = *rec;
					for (unsigned int i = 0; i < rec->mReferences.Size(); ++i)
						mDeletedEdges.Add(rec->mReferences[i]);
				}
			}

			void DeleteRecordCommand::Execute()
			{
				// Remove all forward-ref edges from other records that point to this record
				for (unsigned int i = 0; i < mRegistry.GetCount(); ++i)
				{
					AssetRecord& other = const_cast<AssetRecord&>(mRegistry.GetRecordByIndex(i));
					if (other.mId == mRecordId)
						continue;
					for (unsigned int j = 0; j < other.mReferences.Size(); )
					{
						if (other.mReferences[j].mTargetAssetId == mRecordId)
							other.mReferences.RemoveAt(j);
						else
							++j;
					}
				}

				mRegistry.Remove(mRecordId);
				mRelationships.InvalidateReverseCache();
				mExecuted = true;
			}

			void DeleteRecordCommand::Undo()
			{
				if (!mExecuted)
					return;

				// Restore the record
				mRegistry.Register(mDeletedRecord);

				// Restore forward refs that were in the deleted record's mReferences
				// (they are already in the snapshot, Register restores the record as-is)

				mRelationships.InvalidateReverseCache();
				mExecuted = false;
			}

			const char* DeleteRecordCommand::GetDescription() const
			{
				return "Delete Asset Record";
			}
		}
	}
}
