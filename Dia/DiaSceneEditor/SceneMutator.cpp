#include "DiaSceneEditor/SceneMutator.h"
#include <DiaObservation/Log/DiaLog.h>
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
	}
}
