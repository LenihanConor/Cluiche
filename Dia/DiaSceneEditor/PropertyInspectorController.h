#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		// Right panel: context-sensitive detail view for the selected scene item.
		// - Layer: direct fields (id, sort_order, parallax, sort_policy, enabled) +
		//   list of lights that affect this layer.
		// - Entity/Camera/Light: identity + all blueprint fields (overridden=editable,
		//   non-overridden=dimmed) + instance_data overrides.
		class PropertyInspectorController
		{
		public:
			// Build the property panel JSON for the given selection.
			// sceneRoot    — the full parsed .diascene Json::Value
			// selectionType — "layer" | "entity" | "camera" | "light"
			// selectionId   — the id string of the selected item
			// blueprintBasePath — directory used to resolve relative blueprint file paths
			//                     (typically the directory containing the .diascene file)
			Json::Value BuildPropertyJson(const Json::Value& sceneRoot,
			                             const char*        selectionType,
			                             const char*        selectionId,
			                             const char*        blueprintBasePath = nullptr) const;

			// Build raw blueprint component list (no instance_data overlay).
			// Used for the read-only blueprint defaults tab (T9).
			Json::Value BuildBlueprintDefaultsJson(const char* blueprintId,
			                                       const char* itemType,
			                                       const char* blueprintBasePath) const;

			// Load a blueprint JSON file and return its component array.
			// Returns empty array on failure.
			Json::Value LoadBlueprintComponents(const char* blueprintId,
			                                    const char* blueprintBasePath,
			                                    const char* itemType) const;

		private:
			// Extract the string value from either a plain string or {"value":"..."} wrapper.
			static const char* ExtractStr(const Json::Value& val, char* buf, int bufSize);

			// Build layer properties.
			Json::Value BuildLayerProperties(const Json::Value& layer,
			                                 const Json::Value& scene) const;

			// Build entity/camera/light properties with blueprint field overlay.
			Json::Value BuildBlueprintProperties(const Json::Value& item,
			                                     const char*        itemType,
			                                     const char*        blueprintBasePath) const;

			// Merge blueprint component fields with instance_data overrides.
			// Returns array of field objects with { name, value, overridden } per field.
			Json::Value MergeFields(const Json::Value& blueprintComponents,
			                        const Json::Value& instanceData) const;
		};
	}
}
