#include "DiaAssetCatalogueEditor/Commands/ApplyRulesCommand.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			ApplyRulesCommand::ApplyRulesCommand(
				Dia::AssetCatalogue::AssetRegistry& registry,
				Dia::AssetCatalogue::RelationshipIndex& relationships,
				const Dia::AssetCatalogue::CatalogueRulesEngine& engine)
				: mRegistry(registry)
				, mRelationships(relationships)
				, mEngine(engine)
				, mChangeset()
				, mPreApplySnapshot()
				, mExecuted(false)
			{}

			void ApplyRulesCommand::Execute()
			{
				if (mExecuted)
					return;

				// Snapshot all records before mutating
				for (unsigned int i = 0; i < mRegistry.GetCount() && !mPreApplySnapshot.IsFull(); ++i)
					mPreApplySnapshot.Add(mRegistry.GetRecordByIndex(i));

				mChangeset = mEngine.Apply(mRegistry, mRelationships);
				mExecuted  = true;
			}

			void ApplyRulesCommand::Undo()
			{
				if (!mExecuted)
					return;

				// Restore all snapshotted records
				for (unsigned int i = 0; i < mPreApplySnapshot.Size(); ++i)
				{
					const Dia::AssetCatalogue::AssetRecord& snap = mPreApplySnapshot[i];
					Dia::AssetCatalogue::AssetRecord* live = mRegistry.FindById(snap.mId);
					if (live)
						*live = snap;
				}
				mRelationships.InvalidateReverseCache();
				mExecuted = false;
			}

		} // namespace Editor
	} // namespace AssetCatalogue
} // namespace Dia
