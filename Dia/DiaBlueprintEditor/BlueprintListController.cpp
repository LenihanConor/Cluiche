#include "DiaBlueprintEditor/BlueprintListController.h"
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

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

		const char* BlueprintListController::LabelForTypeId(const Dia::Core::StringCRC& assetTypeId)
		{
			if (assetTypeId == kEntityTypeId) return "Entity";
			if (assetTypeId == kCameraTypeId) return "Camera";
			if (assetTypeId == kLightTypeId)  return "Light";
			return "Unknown";
		}

		Json::Value BlueprintListController::BuildListJson(
			const Dia::AssetCatalogue::AssetRegistry& registry) const
		{
			Json::Value result;
			result["success"] = true;

			// Build one group per blueprint type (preserving order: Entity, Camera, Light)
			static const Dia::Core::StringCRC kOrder[] = { kEntityTypeId, kCameraTypeId, kLightTypeId };
			static const unsigned int kOrderCount = 3;

			Json::Value groups(Json::arrayValue);

			for (unsigned int g = 0; g < kOrderCount; ++g)
			{
				Dia::Core::Containers::DynamicArrayC<const Dia::AssetCatalogue::AssetRecord*, 64> recs;
				registry.QueryByType(kOrder[g], recs);

				if (recs.Size() == 0)
					continue;

				Json::Value group;
				group["label"] = LabelForTypeId(kOrder[g]);
				group["typeId"] = kOrder[g].AsChar();

				Json::Value items(Json::arrayValue);
				for (unsigned int i = 0; i < recs.Size(); ++i)
				{
					Json::Value item;
					item["id"]    = recs[i]->mId.AsChar();
					item["label"] = recs[i]->mId.AsChar();
					item["path"]  = recs[i]->mSourcePath.AsCStr();
					items.append(item);
				}
				group["items"] = items;
				groups.append(group);
			}

			result["groups"] = groups;
			return result;
		}
	}
}
