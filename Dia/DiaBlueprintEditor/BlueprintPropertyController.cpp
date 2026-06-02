#include "DiaBlueprintEditor/BlueprintPropertyController.h"
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <cstring>

namespace Dia
{
	namespace BlueprintEditor
	{
		// Returns the field kind as a string for the UI to map to the right widget.
		static const char* FieldKindStr(Dia::Entity::FieldKind kind)
		{
			switch (kind)
			{
			case Dia::Entity::FieldKind::Primitive:   return "primitive";
			case Dia::Entity::FieldKind::StringId:    return "string_id";
			case Dia::Entity::FieldKind::Math:        return "math";
			case Dia::Entity::FieldKind::AssetHandle: return "asset_handle";
			case Dia::Entity::FieldKind::EntityRef:   return "entity_ref";
			case Dia::Entity::FieldKind::Nested:      return "nested";
			case Dia::Entity::FieldKind::Container:   return "container";
			default:                                   return "unknown";
			}
		}

		Json::Value BlueprintPropertyController::DescribeField(
			const Dia::Entity::ComponentTypeDesc& desc,
			unsigned int fieldIndex,
			const Json::Value& fieldValues)
		{
			const Dia::Entity::FieldDesc& f = desc.fields[fieldIndex];
			Json::Value fieldJson;
			fieldJson["name"]     = f.name;
			fieldJson["kind"]     = FieldKindStr(f.kind);
			fieldJson["required"] = f.required;

			if (fieldValues.isMember(f.name))
				fieldJson["value"] = fieldValues[f.name];

			return fieldJson;
		}

		Json::Value BlueprintPropertyController::BuildPropertyJson(
			const Json::Value& blueprintRoot,
			const char* topLevelKey) const
		{
			DIA_TRACE_ZONE("blueprint_editor.build_property", Dia::Observation::Trace::Category::kNone);
			Json::Value result;
			if (!blueprintRoot.isMember(topLevelKey))
			{
				result["error"] = "top-level key not found";
				return result;
			}

			const Json::Value& bp = blueprintRoot[topLevelKey];
			result["id"] = bp.get("id", "").asString();

			Json::Value componentArray(Json::arrayValue);
			Json::Value components = bp.get("components", Json::Value(Json::arrayValue));

			for (unsigned int i = 0; i < components.size(); ++i)
			{
				const Json::Value& comp = components[i];
				const char* typeName = comp["type"].asCString();
				Json::Value fieldValues = comp.isMember("fields") ? comp["fields"] : Json::Value(Json::objectValue);

				Json::Value compJson;
				compJson["type"] = typeName;

				// Enrich with field metadata from ComponentRegistry if available
				const Dia::Entity::ComponentTypeDesc* desc =
					Dia::Entity::ComponentRegistry::Get().Find(Dia::Core::StringCRC(typeName));

				Json::Value fields(Json::arrayValue);
				if (desc && desc->fieldCount > 0)
				{
					for (unsigned int f = 0; f < desc->fieldCount; ++f)
						fields.append(DescribeField(*desc, f, fieldValues));
				}
				else
				{
					// No registry entry: expose raw JSON fields as-is
					for (auto it = fieldValues.begin(); it != fieldValues.end(); ++it)
					{
						Json::Value fieldJson;
						fieldJson["name"]  = it.name();
						fieldJson["kind"]  = "primitive";
						fieldJson["value"] = *it;
						fields.append(fieldJson);
					}
				}
				compJson["fields"] = fields;
				componentArray.append(compJson);
			}

			result["components"] = componentArray;
			return result;
		}

		Json::Value BlueprintPropertyController::BuildAvailableComponentsJson(
			const Json::Value& blueprintRoot,
			const char* topLevelKey) const
		{
			DIA_TRACE_ZONE("blueprint_editor.build_available_components", Dia::Observation::Trace::Category::kNone);
			Json::Value result(Json::arrayValue);

			// Collect component types already in the blueprint
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> present;
			if (blueprintRoot.isMember(topLevelKey))
			{
				Json::Value comps = blueprintRoot[topLevelKey].get("components",
				                                                    Json::Value(Json::arrayValue));
				for (unsigned int i = 0; i < comps.size(); ++i)
					present.Add(Dia::Core::StringCRC(comps[i].get("type", "").asCString()));
			}

			const Dia::Entity::ComponentRegistry& reg = Dia::Entity::ComponentRegistry::Get();
			for (uint32_t i = 0; i < reg.GetCount(); ++i)
			{
				const Dia::Entity::ComponentTypeDesc& desc = reg.GetByIndex(i);

				bool alreadyPresent = false;
				for (unsigned int j = 0; j < present.Size(); ++j)
				{
					if (present[j] == desc.typeId)
					{
						alreadyPresent = true;
						break;
					}
				}
				if (alreadyPresent)
					continue;

				Json::Value entry;
				entry["typeId"] = desc.typeId.AsChar();
				entry["label"]  = desc.debugName ? desc.debugName : desc.typeId.AsChar();
				if (desc.description)
					entry["description"] = desc.description;
				result.append(entry);
			}

			return result;
		}

		Json::Value BlueprintPropertyController::BuildUsageJson(
			const Dia::Core::StringCRC& blueprintAssetId,
			Dia::AssetCatalogue::AssetRegistry& registry) const
		{
			Json::Value result(Json::objectValue);
			Json::Value usages(Json::arrayValue);

			Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> reverseRefs;
			registry.GetRelationshipIndex().GetReverseRefs(blueprintAssetId, registry, reverseRefs);

			for (unsigned int i = 0; i < reverseRefs.Size(); ++i)
			{
				// GetReverseRefs rebuilds StringCRCs from CRC hashes only — AsChar() is empty.
				// Resolve the display string by scanning the registry by CRC value.
				const Dia::Core::StringCRC& fromCRC = reverseRefs[i].mTargetAssetId;
				const char* sceneIdStr = "";
				for (unsigned int r = 0; r < registry.GetCount(); ++r)
				{
					const Dia::AssetCatalogue::AssetRecord& rec = registry.GetRecordByIndex(r);
					if (rec.mId.Value() == fromCRC.Value())
					{
						sceneIdStr = rec.mId.AsChar();
						break;
					}
				}

				Json::Value entry;
				entry["sceneId"]       = sceneIdStr;
				entry["instanceCount"] = 1;
				usages.append(entry);
			}

			result["usages"] = usages;
			return result;
		}
	}
}
