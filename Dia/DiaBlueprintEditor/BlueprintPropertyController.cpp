#include "DiaBlueprintEditor/BlueprintPropertyController.h"
#include "DiaBlueprintEditor/SchemaReader.h"
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentTypeDesc.h>
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
			const char* topLevelKey,
			const SchemaReader& schema) const
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

				// Inject codeDefault from schema for any field whose name has a default value
				if (schema.IsLoaded())
				{
					const Json::Value* defaultValues = nullptr;
					for (unsigned int s = 0; s < schema.GetComponentCount(); ++s)
					{
						if (schema.GetComponent(s).typeId == Dia::Core::StringCRC(typeName))
						{
							const Json::Value& dv = schema.GetDefaultValues(s);
							if (!dv.isNull())
								defaultValues = &dv;
							break;
						}
					}

					if (defaultValues)
					{
						for (unsigned int fi = 0; fi < fields.size(); ++fi)
						{
							const char* fname = fields[fi]["name"].asCString();
							if (fname && defaultValues->isMember(fname))
								fields[fi]["codeDefault"] = (*defaultValues)[fname];
						}
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
			const char* topLevelKey,
			const SchemaReader& schema) const
		{
			DIA_TRACE_ZONE("blueprint_editor.build_available_components", Dia::Observation::Trace::Category::kNone);
			Json::Value result(Json::arrayValue);

			if (!schema.IsLoaded())
			{
				Json::Value statusEntry;
				statusEntry["statusMessage"] = "No schema found \xe2\x80\x94 run `dia reflect`";
				result.append(statusEntry);
				return result;
			}

			// Collect component types already in the blueprint
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> present;
			if (blueprintRoot.isMember(topLevelKey))
			{
				Json::Value comps = blueprintRoot[topLevelKey].get("components",
				                                                    Json::Value(Json::arrayValue));
				for (unsigned int i = 0; i < comps.size(); ++i)
					present.Add(Dia::Core::StringCRC(comps[i].get("type", "").asCString()));
			}

			for (unsigned int i = 0; i < schema.GetComponentCount(); ++i)
			{
				const SchemaComponentEntry& entry = schema.GetComponent(i);

				bool alreadyPresent = false;
				for (unsigned int j = 0; j < present.Size(); ++j)
				{
					if (present[j] == entry.typeId)
					{
						alreadyPresent = true;
						break;
					}
				}
				if (alreadyPresent)
					continue;

				Json::Value compEntry;
				compEntry["typeId"] = entry.typeId.AsChar();
				compEntry["label"]  = entry.debugName[0] ? entry.debugName : entry.typeId.AsChar();
				result.append(compEntry);
			}

			return result;
		}

		Json::Value BlueprintPropertyController::BuildUsageJson(const Json::Value& reverseRefs) const
		{
			Json::Value result(Json::objectValue);
			Json::Value usages(Json::arrayValue);

			if (reverseRefs.isArray())
			{
				for (unsigned int i = 0; i < reverseRefs.size(); ++i)
				{
					Json::Value entry;
					entry["sceneId"]       = reverseRefs[i].get("source", "").asString();
					entry["instanceCount"] = 1;
					usages.append(entry);
				}
			}

			result["usages"] = usages;
			return result;
		}
	}
}
