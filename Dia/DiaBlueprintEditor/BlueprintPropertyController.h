#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Entity
	{
		struct ComponentTypeDesc;
	}

	namespace BlueprintEditor
	{
		class SchemaReader;

		// Right panel: renders component accordion + field editing + cross-scene usage.
		class BlueprintPropertyController
		{
		public:
			// Build the property panel JSON for a loaded blueprint root.
			// Returns: { "id": "...", "components": [ { "type": "...", "fields": [...] } ] }
			Json::Value BuildPropertyJson(const Json::Value& blueprintRoot,
			                             const char* topLevelKey) const;

			// Build a JSON array of available component types (for Add Component dropdown).
			// Filters out types already present in the blueprint.
			// If schema is not loaded, returns a status message entry instead.
			Json::Value BuildAvailableComponentsJson(const Json::Value& blueprintRoot,
			                                         const char* topLevelKey,
			                                         const SchemaReader& schema) const;

			// Build usage JSON by querying the asset catalogue for reverse refs.
			// reverseRefs is the "refs" array from asset_catalogue.get_reverse_refs.
			// Returns: { "usages": [ { "sceneId": "...", "instanceCount": N } ] }
			Json::Value BuildUsageJson(const Json::Value& reverseRefs) const;

		private:
			static Json::Value DescribeField(const Dia::Entity::ComponentTypeDesc& desc,
			                                 unsigned int fieldIndex,
			                                 const Json::Value& fieldValues);
		};
	}
}
