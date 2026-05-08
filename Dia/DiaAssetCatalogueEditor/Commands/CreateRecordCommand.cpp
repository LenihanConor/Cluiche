#include "DiaAssetCatalogueEditor/Commands/CreateRecordCommand.h"
#include <DiaAssetCatalogue/AssetRegistry.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			CreateRecordCommand::CreateRecordCommand(
				Dia::AssetCatalogue::AssetRegistry& registry,
				const Dia::AssetCatalogue::AssetRecord& record)
				: mRegistry(registry)
				, mRecord(record)
			{
			}

			void CreateRecordCommand::Execute()
			{
				mRegistry.Register(mRecord);
			}

			void CreateRecordCommand::Undo()
			{
				mRegistry.Remove(mRecord.mId);
			}

			const char* CreateRecordCommand::GetDescription() const
			{
				return "Create Asset Record";
			}
		}
	}
}
