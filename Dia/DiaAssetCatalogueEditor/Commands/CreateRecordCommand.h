#pragma once

#include <DiaEditor/Command/IEditorCommand.h>
#include <DiaAssetCatalogue/AssetRecord.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetRegistry;
	}

	namespace AssetCatalogue
	{
		namespace Editor
		{
			class CreateRecordCommand : public Dia::Editor::IEditorCommand
			{
			public:
				CreateRecordCommand(Dia::AssetCatalogue::AssetRegistry& registry,
					const Dia::AssetCatalogue::AssetRecord& record);

				void Execute() override;
				void Undo() override;
				const char* GetDescription() const override;

			private:
				Dia::AssetCatalogue::AssetRegistry& mRegistry;
				Dia::AssetCatalogue::AssetRecord    mRecord;
			};
		}
	}
}
