#include "DiaAssetCatalogueEditor/Commands/AddRelationshipCommand.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			AddRelationshipCommand::AddRelationshipCommand(
				Dia::AssetCatalogue::AssetRegistry& registry,
				const Dia::Core::StringCRC& fromId,
				const Dia::Core::StringCRC& relType,
				const Dia::Core::StringCRC& toId)
				: mRegistry(registry)
				, mFromId(fromId)
				, mEdge(relType, toId)
			{}

			void AddRelationshipCommand::Execute()
			{
				AssetRecord* rec = mRegistry.FindById(mFromId);
				if (rec && !rec->mReferences.IsFull())
					rec->mReferences.Add(mEdge);
				mRegistry.GetRelationshipIndex().InvalidateReverseCache();
			}

			void AddRelationshipCommand::Undo()
			{
				AssetRecord* rec = mRegistry.FindById(mFromId);
				if (!rec) return;

				for (unsigned int i = 0; i < rec->mReferences.Size(); ++i)
				{
					if (rec->mReferences[i].mRelationshipType == mEdge.mRelationshipType &&
					    rec->mReferences[i].mTargetAssetId == mEdge.mTargetAssetId)
					{
						rec->mReferences.RemoveAt(i);
						break;
					}
				}
				mRegistry.GetRelationshipIndex().InvalidateReverseCache();
			}

		} // namespace Editor
	} // namespace AssetCatalogue
} // namespace Dia
