#include "DiaSceneEditor/SceneMutator.h"
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <cstring>
#include <cctype>
#include <cstdio>

namespace Dia
{
	namespace SceneEditor
	{
		// ── Helpers ──────────────────────────────────────────────────────────────────

		const char* SceneMutator::ArrayKey(const char* itemType)
		{
			if (!itemType) return nullptr;
			if (strcmp(itemType, "entity") == 0) return "entities";
			if (strcmp(itemType, "camera") == 0) return "cameras";
			if (strcmp(itemType, "light")  == 0) return "lights";
			if (strcmp(itemType, "layer")  == 0) return "layers";
			return nullptr;
		}

		const char* SceneMutator::ExtractId(const Json::Value& val, char* buf, int bufSize)
		{
			if (val.isString())
			{
				strncpy(buf, val.asCString(), bufSize - 1);
				buf[bufSize - 1] = '\0';
				return buf;
			}
			if (val.isObject() && val.isMember("value") && val["value"].isString())
			{
				strncpy(buf, val["value"].asCString(), bufSize - 1);
				buf[bufSize - 1] = '\0';
				return buf;
			}
			buf[0] = '\0';
			return buf;
		}

		bool SceneMutator::IdExists(const Json::Value& arr, const char* id)
		{
			char buf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				const Json::Value& item = arr[i];
				if (item.isMember("id") && strcmp(ExtractId(item["id"], buf, sizeof(buf)), id) == 0)
					return true;
			}
			return false;
		}

		bool SceneMutator::IsValidId(const char* id)
		{
			if (!id || id[0] == '\0') return false;
			for (const char* p = id; *p; ++p)
			{
				if (!std::isalnum((unsigned char)*p) && *p != '_')
					return false;
			}
			return true;
		}

		// ── T11: AddItem ─────────────────────────────────────────────────────────────

		bool SceneMutator::AddItem(Json::Value& sceneRoot,
		                           const char*  itemType,
		                           const char*  blueprintId,
		                           char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::AddItem", Dia::Observation::Trace::Category::kNone);
			const char* arrayKey = ArrayKey(itemType);
			if (!arrayKey)
			{
				if (errBuf) snprintf(errBuf, errBufSize, "unknown item type: %s", itemType ? itemType : "");
				return false;
			}
			if (!sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "scene2d root missing");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"][arrayKey];
			if (!arr.isArray()) arr = Json::Value(Json::arrayValue);

			// Generate unique id: blueprintId_0, blueprintId_1, ...
			char newId[512];
			for (int n = 0; n < 10000; ++n)
			{
				snprintf(newId, sizeof(newId), "%s_%d", blueprintId ? blueprintId : "item", n);
				if (!IdExists(arr, newId)) break;
			}

			Json::Value item(Json::objectValue);
			item["id"]            = Json::Value(Json::objectValue);
			item["id"]["value"]   = newId;
			item["blueprint"]     = Json::Value(Json::objectValue);
			item["blueprint"]["value"] = blueprintId ? blueprintId : "";
			item["enabled"]       = true;
			item["instance_data"] = Json::Value(Json::objectValue);

			// Camera default: inactive unless it's the first camera
			if (strcmp(itemType, "camera") == 0)
			{
				item["active"] = (arr.size() == 0);
			}
			// Light default: affects all layers
			if (strcmp(itemType, "light") == 0)
			{
				item["affects_layers"] = Json::Value(Json::arrayValue);
			}

			arr.append(item);
			DIA_LOG_INFO("Editor", "SceneMutator: added %s '%s' (blueprint '%s')",
				itemType, newId, blueprintId ? blueprintId : "");
			return true;
		}

		// ── T12: DuplicateItem ───────────────────────────────────────────────────────

		bool SceneMutator::DuplicateItem(Json::Value& sceneRoot,
		                                  const char*  itemType,
		                                  const char*  itemId,
		                                  char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::DuplicateItem", Dia::Observation::Trace::Category::kNone);
			const char* arrayKey = ArrayKey(itemType);
			if (!arrayKey || !sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "invalid type or missing scene2d");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"][arrayKey];
			if (!arr.isArray())
			{
				if (errBuf) snprintf(errBuf, errBufSize, "section '%s' not found", arrayKey);
				return false;
			}

			char idBuf[256];
			int  srcIndex = -1;
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (arr[i].isMember("id") &&
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), itemId) == 0)
				{
					srcIndex = (int)i;
					break;
				}
			}

			if (srcIndex < 0)
			{
				if (errBuf) snprintf(errBuf, errBufSize, "item '%s' not found", itemId);
				return false;
			}

			Json::Value copy = arr[srcIndex];

			// Generate new id: "{oldId}_copy", then _copy2, _copy3 ...
			char newId[512];
			snprintf(newId, sizeof(newId), "%s_copy", itemId);
			if (IdExists(arr, newId))
			{
				for (int n = 2; n < 10000; ++n)
				{
					snprintf(newId, sizeof(newId), "%s_copy%d", itemId, n);
					if (!IdExists(arr, newId)) break;
				}
			}

			// Update id
			if (copy["id"].isObject())
				copy["id"]["value"] = newId;
			else
				copy["id"] = newId;

			// Offset position by +50 if instance_data has Transform2D.position
			if (copy.isMember("instance_data"))
			{
				Json::Value& inst = copy["instance_data"];
				if (inst.isMember("Transform2D.position") && inst["Transform2D.position"].isArray()
				    && inst["Transform2D.position"].size() >= 2)
				{
					inst["Transform2D.position"][0] =
						inst["Transform2D.position"][0].asFloat() + 50.0f;
					inst["Transform2D.position"][1] =
						inst["Transform2D.position"][1].asFloat() + 50.0f;
				}
			}

			arr.append(copy);
			DIA_LOG_INFO("Editor", "SceneMutator: duplicated %s '%s' → '%s'", itemType, itemId, newId);
			return true;
		}

		// ── T13: DeleteItem ──────────────────────────────────────────────────────────

		bool SceneMutator::DeleteItem(Json::Value& sceneRoot,
		                               const char*  itemType,
		                               const char*  itemId,
		                               char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::DeleteItem", Dia::Observation::Trace::Category::kNone);
			const char* arrayKey = ArrayKey(itemType);
			if (!arrayKey || !sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "invalid type or missing scene2d");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"][arrayKey];
			if (!arr.isArray())
			{
				if (errBuf) snprintf(errBuf, errBufSize, "section '%s' not found", arrayKey);
				return false;
			}

			Json::Value newArr(Json::arrayValue);
			char idBuf[256];
			bool found = false;
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (arr[i].isMember("id") &&
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), itemId) == 0)
				{ found = true; continue; }
				newArr.append(arr[i]);
			}

			if (!found)
			{
				if (errBuf) snprintf(errBuf, errBufSize, "item '%s' not found", itemId);
				return false;
			}

			sceneRoot["scene2d"][arrayKey] = newArr;
			DIA_LOG_INFO("Editor", "SceneMutator: deleted %s '%s'", itemType, itemId);
			return true;
		}

		// ── T13: SetEnabled ──────────────────────────────────────────────────────────

		bool SceneMutator::SetEnabled(Json::Value& sceneRoot,
		                               const char*  itemType,
		                               const char*  itemId,
		                               bool         enabled,
		                               char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::SetEnabled", Dia::Observation::Trace::Category::kNone);
			const char* arrayKey = ArrayKey(itemType);
			if (!arrayKey || !sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "invalid type or missing scene2d");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"][arrayKey];
			if (!arr.isArray())
			{
				if (errBuf) snprintf(errBuf, errBufSize, "section '%s' not found", arrayKey);
				return false;
			}

			char idBuf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (arr[i].isMember("id") &&
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), itemId) == 0)
				{
					arr[i]["enabled"] = enabled;
					DIA_LOG_INFO("Editor", "SceneMutator: %s %s '%s'",
						enabled ? "enabled" : "disabled", itemType, itemId);
					return true;
				}
			}

			if (errBuf) snprintf(errBuf, errBufSize, "item '%s' not found", itemId);
			return false;
		}

		// ── T14: RenameItem ──────────────────────────────────────────────────────────

		bool SceneMutator::RenameItem(Json::Value& sceneRoot,
		                               const char*  itemType,
		                               const char*  oldId,
		                               const char*  newId,
		                               char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::RenameItem", Dia::Observation::Trace::Category::kNone);
			if (!IsValidId(newId))
			{
				if (errBuf) snprintf(errBuf, errBufSize,
					"invalid id '%s': use alphanumeric + underscore only", newId ? newId : "");
				return false;
			}

			const char* arrayKey = ArrayKey(itemType);
			if (!arrayKey || !sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "invalid type or missing scene2d");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"][arrayKey];
			if (!arr.isArray())
			{
				if (errBuf) snprintf(errBuf, errBufSize, "section '%s' not found", arrayKey);
				return false;
			}

			if (IdExists(arr, newId))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "id '%s' already in use", newId);
				return false;
			}

			char idBuf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (arr[i].isMember("id") &&
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), oldId) == 0)
				{
					if (arr[i]["id"].isObject())
						arr[i]["id"]["value"] = newId;
					else
						arr[i]["id"] = newId;

					DIA_LOG_INFO("Editor", "SceneMutator: renamed %s '%s' → '%s'",
						itemType, oldId, newId);
					return true;
				}
			}

			if (errBuf) snprintf(errBuf, errBufSize, "item '%s' not found", oldId);
			return false;
		}

		// ── T16: AnalyseChangeBlueprintJson ──────────────────────────────────────────

		Json::Value SceneMutator::AnalyseChangeBlueprintJson(
		    const Json::Value& sceneRoot,
		    const char*        itemType,
		    const char*        itemId,
		    const Json::Value& newBlueprintComponents)
		{
			DIA_TRACE_ZONE("SceneMutator::AnalyseChangeBlueprintJson", Dia::Observation::Trace::Category::kNone);
			Json::Value result(Json::objectValue);
			Json::Value transferred(Json::arrayValue);
			Json::Value orphaned(Json::arrayValue);

			const char* arrayKey = ArrayKey(itemType);
			if (!arrayKey || !sceneRoot.isMember("scene2d"))
				return result;

			const Json::Value& arr = sceneRoot["scene2d"][arrayKey];
			char idBuf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (!arr[i].isMember("id") ||
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), itemId) != 0)
					continue;

				Json::Value instanceData = arr[i].isMember("instance_data")
					? arr[i]["instance_data"] : Json::Value(Json::objectValue);

				// Build set of valid override keys from new blueprint
				// key format: "ComponentType.fieldName"
				for (auto it = instanceData.begin(); it != instanceData.end(); ++it)
				{
					const std::string& key = it.name();
					bool inNewBlueprint = false;

					for (unsigned int ci = 0; ci < newBlueprintComponents.size(); ++ci)
					{
						const Json::Value& comp = newBlueprintComponents[ci];
						if (!comp.isMember("type")) continue;
						std::string prefix = comp["type"].asString() + ".";
						if (key.substr(0, prefix.size()) == prefix)
						{
							// Check field exists in new component
							if (comp.isMember("fields"))
							{
								std::string fieldName = key.substr(prefix.size());
								const Json::Value& fields = comp["fields"];
								for (auto fi = fields.begin(); fi != fields.end(); ++fi)
								{
									if (fi.name() == fieldName) { inNewBlueprint = true; break; }
								}
							}
							else { inNewBlueprint = true; }  // no field list — assume valid
							break;
						}
					}

					if (inNewBlueprint) transferred.append(key);
					else                orphaned.append(key);
				}
				break;
			}

			result["transferCount"] = (int)transferred.size();
			result["orphanCount"]   = (int)orphaned.size();
			result["transferred"]   = transferred;
			result["orphaned"]      = orphaned;
			return result;
		}

		// ── T16: ChangeBlueprint ─────────────────────────────────────────────────────

		bool SceneMutator::ChangeBlueprint(Json::Value& sceneRoot,
		                                    const char*  itemType,
		                                    const char*  itemId,
		                                    const char*  newBlueprintId,
		                                    const Json::Value& newBlueprintComponents,
		                                    char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::ChangeBlueprint", Dia::Observation::Trace::Category::kNone);
			const char* arrayKey = ArrayKey(itemType);
			if (!arrayKey || !sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "invalid type or missing scene2d");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"][arrayKey];
			char idBuf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (!arr[i].isMember("id") ||
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), itemId) != 0)
					continue;

				// Build transferred instance_data (drop orphans)
				Json::Value analysis = AnalyseChangeBlueprintJson(
					sceneRoot, itemType, itemId, newBlueprintComponents);

				Json::Value oldInstanceData = arr[i].isMember("instance_data")
					? arr[i]["instance_data"] : Json::Value(Json::objectValue);
				Json::Value newInstanceData(Json::objectValue);
				const Json::Value& transferred = analysis["transferred"];
				for (unsigned int t = 0; t < transferred.size(); ++t)
				{
					const std::string& k = transferred[t].asString();
					if (oldInstanceData.isMember(k))
						newInstanceData[k] = oldInstanceData[k];
				}

				// Update blueprint id
				if (arr[i]["blueprint"].isObject())
					arr[i]["blueprint"]["value"] = newBlueprintId ? newBlueprintId : "";
				else
					arr[i]["blueprint"] = newBlueprintId ? newBlueprintId : "";

				arr[i]["instance_data"] = newInstanceData;

				DIA_LOG_INFO("Editor", "SceneMutator: changed blueprint of %s '%s' → '%s' "
				             "(transferred=%u orphaned=%u)",
				             itemType, itemId, newBlueprintId ? newBlueprintId : "",
				             transferred.size(), analysis["orphaned"].size());
				return true;
			}

			if (errBuf) snprintf(errBuf, errBufSize, "item '%s' not found", itemId);
			return false;
		}

		// ── T17: Layer CRUD ──────────────────────────────────────────────────────────

		bool SceneMutator::AddLayer(Json::Value& sceneRoot,
		                             const char*  layerId,
		                             char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::AddLayer", Dia::Observation::Trace::Category::kNone);
			if (!IsValidId(layerId))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "invalid layer id '%s'", layerId ? layerId : "");
				return false;
			}
			if (!sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "scene2d root missing");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"]["layers"];
			if (!arr.isArray()) arr = Json::Value(Json::arrayValue);

			if (IdExists(arr, layerId))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "layer '%s' already exists", layerId);
				return false;
			}

			// Default sort_order = last + 10
			int maxOrder = 0;
			for (unsigned int i = 0; i < arr.size(); ++i)
				if (arr[i].isMember("sort_order"))
					maxOrder = std::max(maxOrder, arr[i]["sort_order"].asInt());

			Json::Value layer(Json::objectValue);
			layer["id"]          = Json::Value(Json::objectValue);
			layer["id"]["value"] = layerId;
			layer["sort_order"]  = maxOrder + 10;
			layer["parallax"]    = Json::Value(Json::arrayValue);
			layer["parallax"].append(1.0f);
			layer["parallax"].append(1.0f);
			layer["sort_policy"]          = Json::Value(Json::objectValue);
			layer["sort_policy"]["value"] = "insertion";
			layer["enabled"]              = true;
			arr.append(layer);

			DIA_LOG_INFO("Editor", "SceneMutator: added layer '%s'", layerId);
			return true;
		}

		bool SceneMutator::DeleteLayer(Json::Value& sceneRoot,
		                                const char*  layerId,
		                                char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::DeleteLayer", Dia::Observation::Trace::Category::kNone);
			if (!sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "scene2d root missing");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"]["layers"];
			if (!arr.isArray() || arr.size() <= 1)
			{
				if (errBuf) snprintf(errBuf, errBufSize, "cannot delete last layer");
				return false;
			}

			Json::Value newArr(Json::arrayValue);
			char idBuf[256];
			bool found = false;
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (arr[i].isMember("id") &&
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), layerId) == 0)
				{ found = true; continue; }
				newArr.append(arr[i]);
			}

			if (!found)
			{
				if (errBuf) snprintf(errBuf, errBufSize, "layer '%s' not found", layerId);
				return false;
			}

			// Remove this layer from all lights' affects_layers
			if (sceneRoot["scene2d"].isMember("lights"))
			{
				Json::Value& lights = sceneRoot["scene2d"]["lights"];
				for (unsigned int li = 0; li < lights.size(); ++li)
				{
					if (!lights[li].isMember("affects_layers")) continue;
					Json::Value newAL(Json::arrayValue);
					const Json::Value& al = lights[li]["affects_layers"];
					for (unsigned int j = 0; j < al.size(); ++j)
					{
						char lbuf[256];
						if (strcmp(ExtractId(al[j], lbuf, sizeof(lbuf)), layerId) != 0)
							newAL.append(al[j]);
					}
					lights[li]["affects_layers"] = newAL;
				}
			}

			sceneRoot["scene2d"]["layers"] = newArr;
			DIA_LOG_INFO("Editor", "SceneMutator: deleted layer '%s'", layerId);
			return true;
		}

		bool SceneMutator::ReorderLayer(Json::Value& sceneRoot,
		                                 const char*  layerId,
		                                 int          newIndex,
		                                 char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::ReorderLayer", Dia::Observation::Trace::Category::kNone);
			if (!sceneRoot.isMember("scene2d"))
			{
				if (errBuf) snprintf(errBuf, errBufSize, "scene2d root missing");
				return false;
			}

			Json::Value& arr = sceneRoot["scene2d"]["layers"];
			if (!arr.isArray()) { if (errBuf) snprintf(errBuf, errBufSize, "no layers"); return false; }

			char idBuf[256];
			int srcIdx = -1;
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (arr[i].isMember("id") &&
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), layerId) == 0)
				{ srcIdx = (int)i; break; }
			}

			if (srcIdx < 0) { if (errBuf) snprintf(errBuf, errBufSize, "layer '%s' not found", layerId); return false; }
			if (newIndex < 0) newIndex = 0;
			if (newIndex >= (int)arr.size()) newIndex = (int)arr.size() - 1;
			if (newIndex == srcIdx) return true;

			Json::Value item = arr[srcIdx];
			Json::Value newArr(Json::arrayValue);
			for (unsigned int i = 0; i < arr.size(); ++i)
				if ((int)i != srcIdx) newArr.append(arr[i]);

			Json::Value finalArr(Json::arrayValue);
			for (int i = 0; i < (int)newArr.size(); ++i)
			{
				if (i == newIndex) finalArr.append(item);
				finalArr.append(newArr[i]);
			}
			if (newIndex >= (int)newArr.size()) finalArr.append(item);

			sceneRoot["scene2d"]["layers"] = finalArr;
			DIA_LOG_INFO("Editor", "SceneMutator: reordered layer '%s' to index %d", layerId, newIndex);
			return true;
		}

		bool SceneMutator::UpdateLayer(Json::Value& sceneRoot,
		                                const char*  layerId,
		                                const Json::Value& fields,
		                                char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::UpdateLayer", Dia::Observation::Trace::Category::kNone);
			if (!sceneRoot.isMember("scene2d")) { if (errBuf) snprintf(errBuf, errBufSize, "scene2d root missing"); return false; }

			Json::Value& arr = sceneRoot["scene2d"]["layers"];
			char idBuf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (!arr[i].isMember("id") ||
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), layerId) != 0) continue;

				for (auto it = fields.begin(); it != fields.end(); ++it)
					arr[i][it.name()] = *it;
				DIA_LOG_INFO("Editor", "SceneMutator: updated layer '%s'", layerId);
				return true;
			}
			if (errBuf) snprintf(errBuf, errBufSize, "layer '%s' not found", layerId);
			return false;
		}

		// ── T18: Camera/Light ────────────────────────────────────────────────────────

		bool SceneMutator::SetCameraActive(Json::Value& sceneRoot,
		                                    const char*  cameraId,
		                                    char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::SetCameraActive", Dia::Observation::Trace::Category::kNone);
			if (!sceneRoot.isMember("scene2d")) { if (errBuf) snprintf(errBuf, errBufSize, "scene2d root missing"); return false; }

			Json::Value& arr = sceneRoot["scene2d"]["cameras"];
			if (!arr.isArray()) { if (errBuf) snprintf(errBuf, errBufSize, "no cameras array"); return false; }

			char idBuf[256];
			bool found = false;
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				bool isTarget = arr[i].isMember("id") &&
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), cameraId) == 0;
				arr[i]["active"] = isTarget;
				if (isTarget) found = true;
			}

			if (!found) { if (errBuf) snprintf(errBuf, errBufSize, "camera '%s' not found", cameraId); return false; }
			DIA_LOG_INFO("Editor", "SceneMutator: set camera '%s' active", cameraId);
			return true;
		}

		bool SceneMutator::SetLightAffectsLayers(Json::Value& sceneRoot,
		                                          const char*  lightId,
		                                          const Json::Value& layerIds,
		                                          char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::SetLightAffectsLayers", Dia::Observation::Trace::Category::kNone);
			if (!sceneRoot.isMember("scene2d")) { if (errBuf) snprintf(errBuf, errBufSize, "scene2d root missing"); return false; }

			Json::Value& arr = sceneRoot["scene2d"]["lights"];
			if (!arr.isArray()) { if (errBuf) snprintf(errBuf, errBufSize, "no lights array"); return false; }

			char idBuf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (!arr[i].isMember("id") ||
				    strcmp(ExtractId(arr[i]["id"], idBuf, sizeof(idBuf)), lightId) != 0) continue;

				// Convert plain strings to {"value":"..."} wrappers
				Json::Value wrapped(Json::arrayValue);
				for (unsigned int j = 0; j < layerIds.size(); ++j)
				{
					Json::Value entry(Json::objectValue);
					entry["value"] = layerIds[j].isString()
						? layerIds[j].asString()
						: layerIds[j].get("value", "").asString();
					wrapped.append(entry);
				}
				arr[i]["affects_layers"] = wrapped;
				DIA_LOG_INFO("Editor", "SceneMutator: updated affects_layers for light '%s'", lightId);
				return true;
			}

			if (errBuf) snprintf(errBuf, errBufSize, "light '%s' not found", lightId);
			return false;
		}

		// ── T19: Override management ─────────────────────────────────────────────────

		static Json::Value* FindItem(Json::Value& sceneRoot,
		                              const char* arrayKey, const char* itemId)
		{
			char idBuf[256];
			Json::Value& arr = sceneRoot["scene2d"][arrayKey];
			if (!arr.isArray()) return nullptr;
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				if (arr[i].isMember("id"))
				{
					const Json::Value& idVal = arr[i]["id"];
					const char* v = idVal.isString() ? idVal.asCString()
					              : (idVal.isObject() && idVal.isMember("value")
					                 ? idVal["value"].asCString() : "");
					strncpy(idBuf, v, sizeof(idBuf) - 1); idBuf[sizeof(idBuf)-1] = '\0';
					if (strcmp(idBuf, itemId) == 0) return &arr[i];
				}
			}
			return nullptr;
		}

		bool SceneMutator::AddOverride(Json::Value& sceneRoot,
		                                const char*  itemType,
		                                const char*  itemId,
		                                const char*  overrideKey,
		                                const Json::Value& defaultValue,
		                                char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::AddOverride", Dia::Observation::Trace::Category::kNone);
			const char* ak = ArrayKey(itemType);
			if (!ak || !sceneRoot.isMember("scene2d")) { if (errBuf) snprintf(errBuf, errBufSize, "invalid type or scene"); return false; }

			Json::Value* item = FindItem(sceneRoot, ak, itemId);
			if (!item) { if (errBuf) snprintf(errBuf, errBufSize, "item '%s' not found", itemId); return false; }

			if (!(*item).isMember("instance_data") || (*item)["instance_data"].isNull())
				(*item)["instance_data"] = Json::Value(Json::objectValue);

			(*item)["instance_data"][overrideKey] = defaultValue;
			DIA_LOG_INFO("Editor", "SceneMutator: added override '%s' on %s '%s'", overrideKey, itemType, itemId);
			return true;
		}

		bool SceneMutator::RemoveOverride(Json::Value& sceneRoot,
		                                   const char*  itemType,
		                                   const char*  itemId,
		                                   const char*  overrideKey,
		                                   char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::RemoveOverride", Dia::Observation::Trace::Category::kNone);
			const char* ak = ArrayKey(itemType);
			if (!ak || !sceneRoot.isMember("scene2d")) { if (errBuf) snprintf(errBuf, errBufSize, "invalid type or scene"); return false; }

			Json::Value* item = FindItem(sceneRoot, ak, itemId);
			if (!item) { if (errBuf) snprintf(errBuf, errBufSize, "item '%s' not found", itemId); return false; }

			if ((*item).isMember("instance_data"))
				(*item)["instance_data"].removeMember(overrideKey);

			DIA_LOG_INFO("Editor", "SceneMutator: removed override '%s' on %s '%s'", overrideKey, itemType, itemId);
			return true;
		}

		bool SceneMutator::UpdateOverride(Json::Value& sceneRoot,
		                                   const char*  itemType,
		                                   const char*  itemId,
		                                   const char*  overrideKey,
		                                   const Json::Value& value,
		                                   char* errBuf, int errBufSize)
		{
			DIA_TRACE_ZONE("SceneMutator::UpdateOverride", Dia::Observation::Trace::Category::kNone);
			const char* ak = ArrayKey(itemType);
			if (!ak || !sceneRoot.isMember("scene2d")) { if (errBuf) snprintf(errBuf, errBufSize, "invalid type or scene"); return false; }

			Json::Value* item = FindItem(sceneRoot, ak, itemId);
			if (!item) { if (errBuf) snprintf(errBuf, errBufSize, "item '%s' not found", itemId); return false; }

			if (!(*item).isMember("instance_data") || (*item)["instance_data"].isNull())
				(*item)["instance_data"] = Json::Value(Json::objectValue);

			(*item)["instance_data"][overrideKey] = value;
			DIA_LOG_INFO("Editor", "SceneMutator: updated override '%s' on %s '%s'", overrideKey, itemType, itemId);
			return true;
		}
	}
}
