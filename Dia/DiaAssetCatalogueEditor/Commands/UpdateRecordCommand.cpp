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
					*rec = mNewValues;
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
