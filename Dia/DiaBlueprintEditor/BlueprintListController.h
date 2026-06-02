#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaAssetCatalogue/AssetRegistry.h>

namespace Dia
{
	namespace BlueprintEditor
	{
		// Left panel: lists all registered blueprint assets from the asset catalogue,
		// grouped by type (entity / camera / light).
		class BlueprintListController
		{
		public:
			// Build the grouped list JSON from the registry.
			// Returns: { "groups": [ { "label": "Entity", "items": [ { "id": "...", "label": "...", "path": "..." } ] } ] }
			Json::Value BuildListJson(const Dia::AssetCatalogue::AssetRegistry& registry) const;

			// Returns the IDs of all blueprint assets in the registry that match
			// any of the known blueprint type IDs (.diaentity / .diacamera / .dialight).
			static bool IsBlueprintType(const Dia::Core::StringCRC& assetTypeId);

		private:
			static const char* LabelForTypeId(const Dia::Core::StringCRC& assetTypeId);
		};
	}
}
