#include "DiaAssetCatalogueEditor/Commands/ApplyRulesCommand.h"
#include <DiaAssetCatalogue/RelationshipTypes.h>
#include <string.h>

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
				, mExcludedIds()
				, mChangeset()
				, mPreApplySnapshot()
				, mExecuted(false)
			{}

			void ApplyRulesCommand::AddExcludedId(const Dia::Core::StringCRC& id)
			{
				if (!mExcludedIds.IsFull())
					mExcludedIds.Add(id);
			}

			bool ApplyRulesCommand::IsExcluded(const Dia::Core::StringCRC& id) const
			{
				for (unsigned int i = 0; i < mExcludedIds.Size(); ++i)
				{
					if (mExcludedIds[i] == id)
						return true;
				}
				return false;
			}

			void ApplyRulesCommand::Execute()
			{
				if (mExecuted)
					return;

				for (unsigned int i = 0; i < mRegistry.GetCount() && !mPreApplySnapshot.IsFull(); ++i)
					mPreApplySnapshot.Add(mRegistry.GetRecordByIndex(i));

				mChangeset = mEngine.EvaluateDryRun(mRegistry, mRelationships);

				for (unsigned int i = 0; i < mChangeset.mChanges.Size(); ++i)
				{
					const RuleChange& change = mChangeset.mChanges[i];
					if (IsExcluded(change.mRecordId))
						continue;

					AssetRecord* record = mRegistry.FindById(change.mRecordId);
					if (record == nullptr)
						continue;

					const char* field = change.mField.AsCStr();
					const char* newVal = change.mNewValue.AsCStr();

					if (strcmp(field, "tag") == 0)
					{
						Dia::Core::StringCRC tagCrc(newVal);
						bool found = false;
						for (unsigned int t = 0; t < record->mTags.Size(); ++t)
						{
							if (record->mTags[t] == tagCrc)
							{
								found = true;
								break;
							}
						}
						if (!found && !record->mTags.IsFull())
							record->mTags.Add(tagCrc);
					}
					else if (strcmp(field, "scope") == 0)
					{
						if (strcmp(newVal, "global") == 0)
						{
							record->mScope = AssetScope::kGlobal;
							record->mScopeStageName = Dia::Core::StringCRC();
						}
						else if (strcmp(newVal, "stage") == 0)
						{
							record->mScope = AssetScope::kStage;
							const char* colon = strchr(newVal, ':');
							if (colon != nullptr)
								record->mScopeStageName = Dia::Core::StringCRC(colon + 1);
						}
					}
					else if (strcmp(field, "relationship") == 0)
					{
						Dia::Core::StringCRC stageId(newVal);
						AssetRecord* stageRecord = mRegistry.FindById(stageId);
						if (stageRecord != nullptr)
						{
							RelationshipEdge edge(RelationshipTypes::kContains, change.mRecordId);
							if (!stageRecord->mReferences.IsFull())
								stageRecord->mReferences.Add(edge);
							mRelationships.InvalidateReverseCache();
						}
					}
				}

				mExecuted = true;
			}

			void ApplyRulesCommand::Undo()
			{
				if (!mExecuted)
					return;

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
