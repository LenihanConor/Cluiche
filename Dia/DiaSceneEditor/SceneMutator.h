#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		// Pure JSON mutation helpers for scene CRUD operations.
		// All methods operate on an in-memory sceneRoot and return true on success.
		// errBuf/errBufSize are optional; pass nullptr to skip error output.
		class SceneMutator
		{
		public:
			// T11: Append a new item to the given section array.
			// itemType: "entity" | "camera" | "light"
			// entityTemplateId: the entity template asset id (will be stored as {"value":"..."})
			// itemId: if non-null/non-empty, used as the item id; otherwise auto-generates "{entityTemplateId}_{N}".
			static bool AddItem(Json::Value& sceneRoot,
			                    const char*  itemType,
			                    const char*  entityTemplateId,
			                    char* errBuf, int errBufSize,
			                    const char*  itemId = nullptr);

			// T12: Deep copy an item; new id = "{oldId}_copy" (suffix _copy2, _copy3 if taken).
			// Offsets instance_data Transform2D.position by +50 on both axes if present.
			static bool DuplicateItem(Json::Value& sceneRoot,
			                          const char*  itemType,
			                          const char*  itemId,
			                          char* errBuf, int errBufSize);

			// T13: Remove an item from its array by id.
			static bool DeleteItem(Json::Value& sceneRoot,
			                       const char*  itemType,
			                       const char*  itemId,
			                       char* errBuf, int errBufSize);

			// T13: Set the enabled flag on an item.
			static bool SetEnabled(Json::Value& sceneRoot,
			                       const char*  itemType,
			                       const char*  itemId,
			                       bool         enabled,
			                       char* errBuf, int errBufSize);

			// T14: Rename an item (update its id field).
			// Validates: non-empty, alphanumeric+underscore, unique within section.
			static bool RenameItem(Json::Value& sceneRoot,
			                       const char*  itemType,
			                       const char*  oldId,
			                       const char*  newId,
			                       char* errBuf, int errBufSize);

			// T16: Analyse blueprint change — compares existing instance_data keys against
			// the new entity template's field set.  Returns:
			//   { transferCount, orphanCount,
			//     transferred: [...], orphaned: [...] }
			// Does NOT mutate sceneRoot.
			static Json::Value AnalyseChangeBlueprintJson(
			                       const Json::Value& sceneRoot,
			                       const char*        itemType,
			                       const char*        itemId,
			                       const Json::Value& newEntityTemplateComponents);

			// T16: Apply blueprint change — reassigns entity template id and transfers/drops
			// instance_data overrides according to the analysis.
			static bool ChangeBlueprint(Json::Value& sceneRoot,
			                            const char*  itemType,
			                            const char*  itemId,
			                            const char*  newEntityTemplateId,
			                            const Json::Value& newEntityTemplateComponents,
			                            char* errBuf, int errBufSize);

			// T17: Layer CRUD — add/delete/reorder/update.
			static bool AddLayer(Json::Value& sceneRoot,
			                     const char*  layerId,
			                     char* errBuf, int errBufSize);
			static bool DeleteLayer(Json::Value& sceneRoot,
			                        const char*  layerId,
			                        char* errBuf, int errBufSize);
			static bool ReorderLayer(Json::Value& sceneRoot,
			                         const char*  layerId,
			                         int          newIndex,
			                         char* errBuf, int errBufSize);
			static bool UpdateLayer(Json::Value& sceneRoot,
			                        const char*  layerId,
			                        const Json::Value& fields,
			                        char* errBuf, int errBufSize);

			// T18: Camera active enforcement — sets one camera active, all others inactive.
			static bool SetCameraActive(Json::Value& sceneRoot,
			                            const char*  cameraId,
			                            char* errBuf, int errBufSize);

			// T18: Update light affects_layers list.
			static bool SetLightAffectsLayers(Json::Value& sceneRoot,
			                                  const char*  lightId,
			                                  const Json::Value& layerIds,
			                                  char* errBuf, int errBufSize);

			// T18: Set light type ("DIR" or "PNT").
			static bool SetLightType(Json::Value& sceneRoot,
			                         const char*  lightId,
			                         const char*  type,
			                         char* errBuf, int errBufSize);

			// T19: Add override — copy blueprint default into instance_data for a field.
			// overrideKey is "ComponentType.fieldName"; defaultValue is the blueprint default.
			static bool AddOverride(Json::Value& sceneRoot,
			                        const char*  itemType,
			                        const char*  itemId,
			                        const char*  overrideKey,
			                        const Json::Value& defaultValue,
			                        char* errBuf, int errBufSize);

			// T19: Remove override — delete a key from instance_data.
			static bool RemoveOverride(Json::Value& sceneRoot,
			                           const char*  itemType,
			                           const char*  itemId,
			                           const char*  overrideKey,
			                           char* errBuf, int errBufSize);

			// T19: Update an existing override value.
			static bool UpdateOverride(Json::Value& sceneRoot,
			                           const char*  itemType,
			                           const char*  itemId,
			                           const char*  overrideKey,
			                           const Json::Value& value,
			                           char* errBuf, int errBufSize);

		private:
			static const char* ArrayKey(const char* itemType);
			static const char* ExtractId(const Json::Value& val, char* buf, int bufSize);
			static bool        IdExists(const Json::Value& arr, const char* id);
			static bool        IsValidId(const char* id);
		};
	}
}
