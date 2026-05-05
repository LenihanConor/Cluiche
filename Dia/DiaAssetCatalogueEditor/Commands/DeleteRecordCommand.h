#pragma once

#include <DiaEditor/Command/IEditorCommand.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetRegistry;
		class RelationshipIndex;
	}

	namespace AssetCatalogue
	{
		namespace Editor
		{
			class DeleteRecordCommand : public Dia::Editor::IEditorCommand
			{
			public:
				DeleteRecordCommand(Dia::AssetCatalogue::AssetRegistry& registry,
					Dia::AssetCatalogue::RelationshipIndex& relationships,
					const Dia::Core::StringCRC& recordId);

				void Execute() override;
				void Undo() override;
				const char* GetDescription() const override;

			private:
				struct InboundEdge
				{
					Dia::Core::StringCRC mSourceRecordId;
					Dia::AssetCatalogue::RelationshipEdge mEdge;
				};

				Dia::AssetCatalogue::AssetRegistry&    mRegistry;
				Dia::AssetCatalogue::RelationshipIndex& mRelationships;
				Dia::Core::StringCRC                   mRecordId;
				Dia::AssetCatalogue::AssetRecord        mDeletedRecord;
				Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> mDeletedEdges;
				Dia::Core::Containers::DynamicArrayC<InboundEdge, 16> mInboundEdges;
				bool mExecuted = false;
			};
		}
	}
}
