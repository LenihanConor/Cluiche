#include "DiaSceneEditor/PropertyInspectorController.h"
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/CRC/StringCRC.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>

namespace
{
	// Extension for each blueprint item type.
	static const char* BlueprintExtension(const char* itemType)
	{
		if (!itemType) return ".diaentitytemplate";
		if (strcmp(itemType, "camera") == 0) return ".diacamera";
		if (strcmp(itemType, "light")  == 0) return ".dialight";
		return ".diaentitytemplate";
	}

	// Top-level JSON key for each blueprint extension.
	static const char* TopKeyForType(const char* itemType)
	{
		if (!itemType) return "entity_template";
		if (strcmp(itemType, "camera") == 0) return "camera_blueprint";
		if (strcmp(itemType, "light")  == 0) return "light_blueprint";
		return "entity_template";
	}

	static bool ReadJson(const char* path, Json::Value& out)
	{
		std::ifstream f(path);
		if (!f.is_open()) return false;
		std::string content((std::istreambuf_iterator<char>(f)),
		                     std::istreambuf_iterator<char>());
		Json::CharReaderBuilder b;
		std::string parseErr;
		std::istringstream ss(content);
		return Json::parseFromStream(b, ss, &out, &parseErr);
	}

	// Normalise dir separator to '/' and strip trailing slash.
	static void NormaliseDir(const char* path, char* out, int outSize)
	{
		strncpy_s(out, outSize, path, _TRUNCATE);
		for (char* p = out; *p; ++p) if (*p == '\\') *p = '/';
		int len = (int)strlen(out);
		if (len > 0 && out[len - 1] == '/') out[len - 1] = '\0';
	}
}

namespace Dia
{
	namespace SceneEditor
	{
		// static
		const char* PropertyInspectorController::ExtractStr(const Json::Value& val, char* buf, int bufSize)
		{
			if (val.isString())
			{
				strncpy_s(buf, bufSize, val.asCString(), _TRUNCATE);
				return buf;
			}
			if (val.isObject() && val.isMember("value") && val["value"].isString())
			{
				strncpy_s(buf, bufSize, val["value"].asCString(), _TRUNCATE);
				return buf;
			}
			buf[0] = '\0';
			return buf;
		}

		// ── Layer (T6) ──────────────────────────────────────────────────────────────

		Json::Value PropertyInspectorController::BuildLayerProperties(
			const Json::Value& layer,
			const Json::Value& scene) const
		{
			Json::Value result(Json::objectValue);
			result["selectionType"] = "layer";

			char buf[256];
			result["id"]          = ExtractStr(layer.isMember("id") ? layer["id"] : Json::Value(""), buf, sizeof(buf));
			result["sort_order"]  = layer.isMember("sort_order")  ? layer["sort_order"]  : Json::Value(0);
			result["parallax"]    = layer.isMember("parallax")    ? layer["parallax"]    : Json::Value(Json::arrayValue);
			result["sort_policy"] = layer.isMember("sort_policy")
				? Json::Value(ExtractStr(layer["sort_policy"], buf, sizeof(buf)))
				: Json::Value("insertion");
			result["enabled"]     = layer.isMember("enabled") ? layer["enabled"] : Json::Value(true);

			// Which lights have this layer in their affects_layers list
			const char* layerId = result["id"].asCString();
			Json::Value affectedBy(Json::arrayValue);
			if (scene.isMember("lights") && scene["lights"].isArray())
			{
				const Json::Value& lights = scene["lights"];
				for (unsigned int i = 0; i < lights.size(); ++i)
				{
					const Json::Value& light = lights[i];
					if (!light.isMember("affects_layers")) continue;
					const Json::Value& al = light["affects_layers"];
					for (unsigned int j = 0; j < al.size(); ++j)
					{
						char lbuf[256];
						if (strcmp(ExtractStr(al[j], lbuf, sizeof(lbuf)), layerId) == 0)
						{
							char nameBuf[256];
							affectedBy.append(ExtractStr(
								light.isMember("id") ? light["id"] : Json::Value(""), nameBuf, sizeof(nameBuf)));
							break;
						}
					}
				}
			}
			result["affected_by_lights"] = affectedBy;

			return result;
		}

		// ── Blueprint overlay (T7/T8) ────────────────────────────────────────────────

		Json::Value PropertyInspectorController::LoadBlueprintComponents(
			const char* entityTemplateId,
			const char* entityTemplateBasePath,
			const char* itemType) const
		{
			if (!entityTemplateId || entityTemplateId[0] == '\0' || !entityTemplateBasePath || entityTemplateBasePath[0] == '\0')
				return Json::Value(Json::arrayValue);

			char dir[512];
			NormaliseDir(entityTemplateBasePath, dir, sizeof(dir));

			// Strip type prefix (e.g. "diaentitytemplate.hero" -> "hero")
			const char* shortName = entityTemplateId;
			const char* lastDot = strrchr(entityTemplateId, '.');
			if (lastDot && lastDot != entityTemplateId) shortName = lastDot + 1;

			char path[768];
			snprintf(path, sizeof(path), "%s/%s%s", dir, shortName, BlueprintExtension(itemType));

			Json::Value root;
			if (!ReadJson(path, root))
			{
				DIA_LOG_WARNING("Editor", "PropertyInspectorController: cannot load entity template '%s'", path);
				return Json::Value(Json::arrayValue);
			}

			const char* topKey = TopKeyForType(itemType);
			if (!root.isMember(topKey)) return Json::Value(Json::arrayValue);

			const Json::Value& bp = root[topKey];
			return bp.isMember("components") ? bp["components"] : Json::Value(Json::arrayValue);
		}

		Json::Value PropertyInspectorController::MergeFields(
			const Json::Value& entityTemplateComponents,
			const Json::Value& instanceData) const
		{
			Json::Value result(Json::arrayValue);

			for (unsigned int ci = 0; ci < entityTemplateComponents.size(); ++ci)
			{
				const Json::Value& comp    = entityTemplateComponents[ci];
				const char*        typeName = comp.isMember("type") ? comp["type"].asCString() : "";
				Json::Value        fieldValues = comp.isMember("fields")
					? comp["fields"] : Json::Value(Json::objectValue);

				Json::Value compJson(Json::objectValue);
				compJson["type"] = typeName;

				// Enrich with ComponentRegistry metadata where available
				const Dia::Entity::ComponentTypeDesc* desc =
					Dia::Entity::ComponentRegistry::Get().Find(Dia::Core::StringCRC(typeName));

				Json::Value fields(Json::arrayValue);

				auto buildField = [&](const char* name, const Json::Value& entityTemplateVal)
				{
					// Override key in instance_data is "ComponentType.fieldName"
					char overrideKey[384];
					snprintf(overrideKey, sizeof(overrideKey), "%s.%s", typeName, name);

					bool overridden = instanceData.isMember(overrideKey);
					Json::Value fieldJson(Json::objectValue);
					fieldJson["name"]       = name;
					fieldJson["overridden"] = overridden;
					fieldJson["value"]      = overridden ? instanceData[overrideKey] : entityTemplateVal;

					if (desc)
					{
						for (unsigned int fi = 0; fi < desc->fieldCount; ++fi)
						{
							if (strcmp(desc->fields[fi].name, name) == 0)
							{
								switch (desc->fields[fi].kind)
								{
								case Dia::Entity::FieldKind::Primitive:   fieldJson["kind"] = "primitive";   break;
								case Dia::Entity::FieldKind::StringId:    fieldJson["kind"] = "string_id";   break;
								case Dia::Entity::FieldKind::Math:        fieldJson["kind"] = "math";        break;
								case Dia::Entity::FieldKind::AssetHandle: fieldJson["kind"] = "asset_handle"; break;
								case Dia::Entity::FieldKind::EntityRef:   fieldJson["kind"] = "entity_ref";  break;
								default:                                   fieldJson["kind"] = "primitive";   break;
								}
								break;
							}
						}
					}
					if (!fieldJson.isMember("kind")) fieldJson["kind"] = "primitive";
					fields.append(fieldJson);
				};

				if (desc && desc->fieldCount > 0)
				{
					for (unsigned int fi = 0; fi < desc->fieldCount; ++fi)
					{
						const char* fname = desc->fields[fi].name;
						Json::Value bpVal = fieldValues.isMember(fname)
							? fieldValues[fname] : Json::Value(Json::nullValue);
						buildField(fname, bpVal);
					}
				}
				else
				{
					// No registry: walk raw JSON fields
					for (auto it = fieldValues.begin(); it != fieldValues.end(); ++it)
						buildField(it.name().c_str(), *it);
				}

				compJson["fields"] = fields;
				result.append(compJson);
			}
			return result;
		}

		Json::Value PropertyInspectorController::BuildBlueprintProperties(
			const Json::Value& item,
			const char*        itemType,
			const char*        entityTemplateBasePath) const
		{
			Json::Value result(Json::objectValue);
			result["selectionType"] = itemType;

			char idBuf[256];
			result["id"] = ExtractStr(item.isMember("id") ? item["id"] : Json::Value(""), idBuf, sizeof(idBuf));

			char etBuf[256];
			const char* entityTemplateId = ExtractStr(
				item.isMember("blueprint") ? item["blueprint"] : Json::Value(""), etBuf, sizeof(etBuf));
			result["entityTemplate"] = entityTemplateId;
			result["enabled"]   = item.isMember("enabled") ? item["enabled"] : Json::Value(true);

			// Load entity template component definitions
			Json::Value etComponents = LoadBlueprintComponents(entityTemplateId, entityTemplateBasePath, itemType);
			result["entityTemplate_known"] = entityTemplateId[0] != '\0'
				? !etComponents.empty()
				: true;

			// Merge with instance_data overrides
			Json::Value instanceData = item.isMember("instance_data")
				? item["instance_data"] : Json::Value(Json::objectValue);
			result["components"] = MergeFields(etComponents, instanceData);

			// Camera-specific: active flag (T8)
			if (strcmp(itemType, "camera") == 0)
				result["active"] = item.isMember("active") ? item["active"] : Json::Value(false);

			// Light-specific: affects_layers (T8)
			if (strcmp(itemType, "light") == 0)
				result["affects_layers"] = item.isMember("affects_layers")
					? item["affects_layers"] : Json::Value(Json::arrayValue);

			return result;
		}

		// ── Main entry point ─────────────────────────────────────────────────────────

		Json::Value PropertyInspectorController::BuildPropertyJson(
			const Json::Value& sceneRoot,
			const char*        selectionType,
			const char*        selectionId,
			const char*        entityTemplateBasePath) const
		{
			DIA_TRACE_ZONE("PropertyInspectorController::BuildPropertyJson", Dia::Observation::Trace::Category::kNone);

			Json::Value empty(Json::objectValue);
			if (!sceneRoot.isMember("scene2d") || !selectionType || !selectionId)
				return empty;

			const Json::Value& scene = sceneRoot["scene2d"];

			const char* arrayKey = nullptr;
			if      (strcmp(selectionType, "entity") == 0) arrayKey = "entities";
			else if (strcmp(selectionType, "camera") == 0) arrayKey = "cameras";
			else if (strcmp(selectionType, "light")  == 0) arrayKey = "lights";
			else if (strcmp(selectionType, "layer")  == 0) arrayKey = "layers";

			if (!arrayKey || !scene.isMember(arrayKey))
				return empty;

			const Json::Value& arr = scene[arrayKey];
			char idBuf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				const Json::Value& item = arr[i];
				const char* itemId = ExtractStr(
					item.isMember("id") ? item["id"] : Json::Value(""), idBuf, sizeof(idBuf));

				if (strcmp(itemId, selectionId) != 0)
					continue;

				if (strcmp(selectionType, "layer") == 0)
					return BuildLayerProperties(item, scene);

				return BuildBlueprintProperties(item, selectionType, entityTemplateBasePath);
			}

			return empty;
		}

		// ── Blueprint defaults (T9) ──────────────────────────────────────────────────

		Json::Value PropertyInspectorController::BuildBlueprintDefaultsJson(
			const char* entityTemplateId,
			const char* itemType,
			const char* entityTemplateBasePath) const
		{
			DIA_TRACE_ZONE("PropertyInspectorController::BuildBlueprintDefaultsJson", Dia::Observation::Trace::Category::kNone);

			Json::Value result(Json::objectValue);
			result["entityTemplateId"] = entityTemplateId ? entityTemplateId : "";
			result["readonly"]    = true;

			Json::Value etComponents = LoadBlueprintComponents(entityTemplateId, entityTemplateBasePath, itemType);

			// Return fields without override overlay — all overridden=false
			Json::Value empty(Json::objectValue);
			result["components"] = MergeFields(etComponents, empty);
			return result;
		}
	}
}
