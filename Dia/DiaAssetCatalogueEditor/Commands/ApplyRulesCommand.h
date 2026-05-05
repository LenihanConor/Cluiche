#pragma once

#include <DiaEditor/Command/IEditorCommand.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/CatalogueRulesEngine.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			// Wraps CatalogueRulesEngine::Apply() as an undoable command.
			// Snapshots all affected records before Apply; Undo restores them.
			class ApplyRulesCommand : public Dia::Editor::IEditorCommand
			{
			public:
				static const unsigned int kMaxExcluded = 32;

				ApplyRulesCommand(
					Dia::AssetCatalogue::AssetRegistry& registry,
					Dia::AssetCatalogue::RelationshipIndex& relationships,
					const Dia::AssetCatalogue::CatalogueRulesEngine& engine);

				void AddExcludedId(const Dia::Core::StringCRC& id);

				void Execute() override;
				void Undo()    override;
				const char* GetDescription() const override { return "Apply Catalogue Rules"; }

				const Dia::AssetCatalogue::RuleChangeset& GetChangeset() const { return mChangeset; }

			private:
				bool IsExcluded(const Dia::Core::StringCRC& id) const;

				Dia::AssetCatalogue::AssetRegistry&             mRegistry;
				Dia::AssetCatalogue::RelationshipIndex&         mRelationships;
				const Dia::AssetCatalogue::CatalogueRulesEngine& mEngine;

				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxExcluded> mExcludedIds;
				Dia::AssetCatalogue::RuleChangeset mChangeset;

				static const unsigned int kMaxSnapshots = 128;
				Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::AssetRecord, kMaxSnapshots> mPreApplySnapshot;
				bool mExecuted;
			};

		} // namespace Editor
	} // namespace AssetCatalogue
} // namespace Dia
