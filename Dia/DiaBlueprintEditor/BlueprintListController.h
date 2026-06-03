#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace BlueprintEditor
	{
		// Left panel: lists all registered blueprint assets from the asset catalogue,
		// grouped by type (entity / camera / light).
		class BlueprintListController
		{
		public:
			// Build the grouped list JSON from per-type record arrays returned by
			// asset_catalogue.query_by_type. Each array contains {id, source_path} objects.
			// Returns: { "groups": [ { "label": "Entity", "items": [ { "id": "...", "label": "...", "path": "..." } ] } ] }
			Json::Value BuildListJson(
				const Json::Value& entityRecords,
				const Json::Value& cameraRecords,
				const Json::Value& lightRecords) const;

			static bool IsBlueprintType(const Dia::Core::StringCRC& assetTypeId);

		private:
			static Json::Value BuildGroup(const char* label, const char* typeId,
			                              const Json::Value& records);
		};
	}
}
