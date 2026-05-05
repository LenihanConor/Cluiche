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
				mInboundEdges.RemoveAll();

				for (unsigned int i = 0; i < mRegistry.GetCount(); ++i)
				{
					AssetRecord& other = const_cast<AssetRecord&>(mRegistry.GetRecordByIndex(i));
					if (other.mId == mRecordId)
						continue;
					for (unsigned int j = 0; j < other.mReferences.Size(); )
					{
						if (other.mReferences[j].mTargetAssetId == mRecordId)
						{
							InboundEdge snapshot;
							snapshot.mSourceRecordId = other.mId;
							snapshot.mEdge = other.mReferences[j];
							if (!mInboundEdges.IsFull())
								mInboundEdges.Add(snapshot);
							other.mReferences.RemoveAt(j);
						}
						else
						{
							++j;
						}
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

				mRegistry.Register(mDeletedRecord);

				for (unsigned int i = 0; i < mInboundEdges.Size(); ++i)
				{
					AssetRecord* source = mRegistry.FindById(mInboundEdges[i].mSourceRecordId);
					if (source && !source->mReferences.IsFull())
						source->mReferences.Add(mInboundEdges[i].mEdge);
				}

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
