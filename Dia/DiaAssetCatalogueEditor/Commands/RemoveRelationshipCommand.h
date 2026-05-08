#pragma once

#include <DiaEditor/Command/IEditorCommand.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			class RemoveRelationshipCommand : public Dia::Editor::IEditorCommand
			{
			public:
				RemoveRelationshipCommand(
					Dia::AssetCatalogue::AssetRegistry& registry,
					const Dia::Core::StringCRC& fromId,
					const Dia::Core::StringCRC& relType,
					const Dia::Core::StringCRC& toId);

				void Execute() override;
				void Undo()    override;
				const char* GetDescription() const override { return "Remove Relationship"; }

			private:
				Dia::AssetCatalogue::AssetRegistry& mRegistry;
				Dia::Core::StringCRC                mFromId;
				RelationshipEdge                    mEdge;
			};

		} // namespace Editor
	} // namespace AssetCatalogue
} // namespace Dia
