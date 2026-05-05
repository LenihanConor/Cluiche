#pragma once

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetRegistry;
		class CatalogueManifestSerializer;
	}

	namespace Editor
	{
		class CommandHistory;
	}

	namespace AssetCatalogue
	{
		namespace Editor
		{
			class ManifestLoadHandler
			{
			public:
				// Load manifest from path into registry. Clears CommandHistory. Returns false on error (errorOut filled).
				bool Load(const char* path,
					Dia::AssetCatalogue::AssetRegistry& registry,
					const Dia::AssetCatalogue::CatalogueManifestSerializer& serializer,
					Dia::Editor::CommandHistory& history,
					char* errorOut, unsigned int errorCapacity);

				// Save current registry to path. Marks CommandHistory save point. Returns false on error.
				bool Save(const char* path,
					const Dia::AssetCatalogue::AssetRegistry& registry,
					const Dia::AssetCatalogue::CatalogueManifestSerializer& serializer,
					Dia::Editor::CommandHistory& history,
					char* errorOut, unsigned int errorCapacity);

				// Reset to empty state (no path). Clears registry and CommandHistory.
				void NewManifest(Dia::AssetCatalogue::AssetRegistry& registry,
					Dia::Editor::CommandHistory& history);

				bool IsDirty(const Dia::Editor::CommandHistory& history) const;
			};
		}
	}
}
