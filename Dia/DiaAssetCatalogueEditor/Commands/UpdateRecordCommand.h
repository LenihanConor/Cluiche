#pragma once

#include <DiaEditor/Command/IEditorCommand.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaCore/CRC/StringCRC.h>

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
			class UpdateRecordCommand : public Dia::Editor::IEditorCommand
			{
			public:
				UpdateRecordCommand(Dia::AssetCatalogue::AssetRegistry& registry,
					const Dia::Core::StringCRC& recordId,
					const Dia::AssetCatalogue::AssetRecord& oldValues,
					const Dia::AssetCatalogue::AssetRecord& newValues);

				void Execute() override;
				void Undo() override;
				const char* GetDescription() const override;

			private:
				Dia::AssetCatalogue::AssetRegistry& mRegistry;
				Dia::Core::StringCRC               mRecordId;
				Dia::AssetCatalogue::AssetRecord    mOldValues;
				Dia::AssetCatalogue::AssetRecord    mNewValues;
			};
		}
	}
}
