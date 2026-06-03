#include "DiaBlueprintEditor/BlueprintListController.h"
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Dia
{
	namespace BlueprintEditor
	{
		static const Dia::Core::StringCRC kEntityTypeId("diaentity");
		static const Dia::Core::StringCRC kCameraTypeId("diacamera");
		static const Dia::Core::StringCRC kLightTypeId("dialight");

		bool BlueprintListController::IsBlueprintType(const Dia::Core::StringCRC& assetTypeId)
		{
			return assetTypeId == kEntityTypeId
			    || assetTypeId == kCameraTypeId
			    || assetTypeId == kLightTypeId;
		}

		Json::Value BlueprintListController::BuildGroup(
			const char* label, const char* typeId, const Json::Value& records)
		{
			Json::Value items(Json::arrayValue);
			for (unsigned int i = 0; i < records.size(); ++i)
			{
				const Json::Value& rec = records[i];
				Json::Value item;
				item["id"]    = rec.get("id", "").asString();
				item["label"] = rec.get("id", "").asString();
				item["path"]  = rec.get("source_path", "").asString();
				items.append(item);
			}

			Json::Value group;
			group["label"]  = label;
			group["typeId"] = typeId;
			group["items"]  = items;
			return group;
		}

		Json::Value BlueprintListController::BuildListJson(
			const Json::Value& entityRecords,
			const Json::Value& cameraRecords,
			const Json::Value& lightRecords) const
		{
			DIA_TRACE_ZONE("blueprint_editor.build_list", Dia::Observation::Trace::Category::kNone);
			Json::Value result;
			result["success"] = true;

			Json::Value groups(Json::arrayValue);
			if (entityRecords.isArray() && entityRecords.size() > 0)
				groups.append(BuildGroup("Entity", "diaentity", entityRecords));
			if (cameraRecords.isArray() && cameraRecords.size() > 0)
				groups.append(BuildGroup("Camera", "diacamera", cameraRecords));
			if (lightRecords.isArray() && lightRecords.size() > 0)
				groups.append(BuildGroup("Light",  "dialight",  lightRecords));

			result["groups"] = groups;
			return result;
		}
	}
}
