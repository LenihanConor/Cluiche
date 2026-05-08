#include "DiaAssetCatalogue/ScopeComputer.h"
#include "DiaAssetCatalogue/AssetRegistry.h"
#include "DiaAssetCatalogue/RelationshipIndex.h"
#include "DiaAssetCatalogue/RelationshipTypes.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// ComputeScope
		//------------------------------------------------------------------------------------
		AssetScope ScopeComputer::ComputeScope(const Dia::Core::StringCRC& assetId,
			const AssetRegistry& registry,
			RelationshipIndex& relationships,
			Dia::Core::StringCRC& outStageName) const
		{
			outStageName = Dia::Core::StringCRC();

			// Check if the asset itself is a Stage — skip it
			const AssetRecord* record = registry.FindById(assetId);
			if (record == nullptr)
			{
				return AssetScope::kGlobal;
			}

			static const Dia::Core::StringCRC kStageTypeId("stage");
			if (record->mAssetTypeId == kStageTypeId)
			{
				// Stage records themselves are not scoped
				return AssetScope::kGlobal;
			}

			// Query reverse refs of type "contains" — these are Stage records that contain this asset
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> containingIds;
			relationships.GetReverseRefsByType(assetId, RelationshipTypes::kContains, registry, containingIds);

			// Count distinct Stage records among the containers
			unsigned int stageCount = 0;
			Dia::Core::StringCRC singleStageName;

			for (unsigned int i = 0; i < containingIds.Size(); ++i)
			{
				const AssetRecord* containerRecord = registry.FindById(containingIds[i]);
				if (containerRecord != nullptr && containerRecord->mAssetTypeId == kStageTypeId)
				{
					++stageCount;
					singleStageName = containingIds[i]; // last stage ID found
				}
			}

			if (stageCount == 1)
			{
				outStageName = singleStageName;
				return AssetScope::kStage;
			}

			// 0 or >1 stages = global
			return AssetScope::kGlobal;
		}

		//------------------------------------------------------------------------------------
		// RecomputeAllScopes
		//------------------------------------------------------------------------------------
		unsigned int ScopeComputer::RecomputeAllScopes(AssetRegistry& registry,
			RelationshipIndex& relationships) const
		{
			unsigned int updatedCount = 0;
			static const Dia::Core::StringCRC kStageTypeId("stage");

			for (unsigned int i = 0; i < registry.GetCount(); ++i)
			{
				const AssetRecord& constRecord = registry.GetRecordByIndex(i);

				// Skip Stage records
				if (constRecord.mAssetTypeId == kStageTypeId)
				{
					continue;
				}

				Dia::Core::StringCRC stageName;
				AssetScope newScope = ComputeScope(constRecord.mId, registry, relationships, stageName);

				// Get mutable record to update
				AssetRecord* mutableRecord = registry.FindById(constRecord.mId);
				if (mutableRecord != nullptr)
				{
					mutableRecord->mScope = newScope;
					mutableRecord->mScopeStageName = stageName;
					++updatedCount;
				}
			}

			return updatedCount;
		}

	} // namespace AssetCatalogue
} // namespace Dia
