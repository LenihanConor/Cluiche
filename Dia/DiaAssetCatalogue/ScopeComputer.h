#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaAssetCatalogue/AssetRecord.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetRegistry;
		class RelationshipIndex;

		//---------------------------------------------------------------------------------------------------------
		// ScopeComputer
		//
		// Computes asset scope (global vs. stage-scoped) by traversing Stage-to-Asset "contains"
		// relationships in the RelationshipIndex.
		//
		// Logic:
		//   - Query reverse refs of type "contains" for the asset
		//   - Count distinct Stage records that contain this asset
		//   - 0 or >1 Stages: kGlobal, outStageName = ""
		//   - Exactly 1 Stage: kStage, outStageName = that stage's name
		//   - Stage records themselves are skipped (not scoped)
		//---------------------------------------------------------------------------------------------------------
		class ScopeComputer
		{
		public:
			// Compute scope for a single asset based on Stage reference count.
			// 1 Stage = kStage (with stage name), 0 or >1 = kGlobal.
			// Stage records themselves are skipped (returns kGlobal with empty stage name).
			AssetScope ComputeScope(const Dia::Core::StringCRC& assetId,
				const AssetRegistry& registry,
				RelationshipIndex& relationships,
				Dia::Core::StringCRC& outStageName) const;

			// Batch-update all records in the registry.
			// Returns count of records updated.
			unsigned int RecomputeAllScopes(AssetRegistry& registry,
				RelationshipIndex& relationships) const;
		};

	} // namespace AssetCatalogue
} // namespace Dia
