#include "DiaAssetCatalogueEditor/Commands/UpdateRecordCommand.h"
#include <DiaAssetCatalogue/AssetRegistry.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			UpdateRecordCommand::UpdateRecordCommand(
				Dia::AssetCatalogue::AssetRegistry& registry,
				const Dia::Core::StringCRC& recordId,
				const Dia::AssetCatalogue::AssetRecord& oldValues,
				const Dia::AssetCatalogue::AssetRecord& newValues)
				: mRegistry(registry)
				, mRecordId(recordId)
				, mOldValues(oldValues)
				, mNewValues(newValues)
			{
			}

			void UpdateRecordCommand::Execute()
			{
				AssetRecord* rec = mRegistry.FindById(mRecordId);
				if (rec)
				{
					// Determine which tracked fields changed and set manual override flags
					unsigned char overrideFlags = rec->mManualOverrideFlags;

					if (mOldValues.mScope != mNewValues.mScope)
						overrideFlags |= kManualOverrideScope;

					if (!(mOldValues.mSourcePath == mNewValues.mSourcePath))
						overrideFlags |= kManualOverrideSourcePath;

					if (mOldValues.mScopeStageName != mNewValues.mScopeStageName)
						overrideFlags |= kManualOverrideStage;

					// Tags: check if the set changed
					bool tagsChanged = false;
					if (mOldValues.mTags.Size() != mNewValues.mTags.Size())
					{
						tagsChanged = true;
					}
					else
					{
						for (unsigned int i = 0; i < mOldValues.mTags.Size(); ++i)
						{
							if (!(mOldValues.mTags[i] == mNewValues.mTags[i]))
							{
								tagsChanged = true;
								break;
							}
						}
					}
					if (tagsChanged)
						overrideFlags |= kManualOverrideTags;

					*rec = mNewValues;
					rec->mManualOverrideFlags = overrideFlags;
				}
			}

			void UpdateRecordCommand::Undo()
			{
				AssetRecord* rec = mRegistry.FindById(mRecordId);
				if (rec)
					*rec = mOldValues;
			}

			const char* UpdateRecordCommand::GetDescription() const
			{
				return "Update Asset Record";
			}
		}
	}
}
