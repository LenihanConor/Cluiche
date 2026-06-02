#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaAssetCatalogue/AssetRegistry.h>

namespace Dia
{
	namespace Entity
	{
		struct ComponentTypeDesc;
	}

	namespace BlueprintEditor
	{
		// Right panel: renders component accordion + field editing + cross-scene usage.
		class BlueprintPropertyController
		{
		public:
			// Build the property panel JSON for a loaded blueprint root.
			// Returns: { "id": "...", "components": [ { "type": "...", "fields": [...] } ] }
			Json::Value BuildPropertyJson(const Json::Value& blueprintRoot,
			                             const char* topLevelKey) const;

			// Build a JSON array of all registered component types (for Add Component dropdown).
			// Filters out types already present in the blueprint.
			Json::Value BuildAvailableComponentsJson(const Json::Value& blueprintRoot,
			                                         const char* topLevelKey) const;

			// Build usage JSON from the asset registry.
			// Returns: { "usages": [ { "sceneId": "...", "instanceCount": N } ] }
			Json::Value BuildUsageJson(const Dia::Core::StringCRC& blueprintAssetId,
			                           Dia::AssetCatalogue::AssetRegistry& registry) const;

		private:
			static Json::Value DescribeField(const Dia::Entity::ComponentTypeDesc& desc,
			                                 unsigned int fieldIndex,
			                                 const Json::Value& fieldValues);
		};
	}
}
