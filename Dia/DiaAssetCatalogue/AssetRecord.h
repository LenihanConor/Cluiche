#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String256.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// AssetStatus
		//---------------------------------------------------------------------------------------------------------
		enum class AssetStatus
		{
			Active,
			Draft,
			Deprecated
		};

		//---------------------------------------------------------------------------------------------------------
		// AssetScope
		//---------------------------------------------------------------------------------------------------------
		enum class AssetScope
		{
			kGlobal,	// Asset shared across multiple Stages — deploys to global/
			kStage		// Asset belongs to exactly one Stage — deploys to stages/<StageName>/
		};

		//---------------------------------------------------------------------------------------------------------
		// ManualOverrideField
		//
		// Bit positions for tracking which fields were manually set by the user (not by a rule).
		// Used as flags in AssetRecord::mManualOverrideFlags.
		//---------------------------------------------------------------------------------------------------------
		enum ManualOverrideField : unsigned char
		{
			kManualOverrideScope      = 1 << 0,
			kManualOverrideTags       = 1 << 1,
			kManualOverrideSourcePath = 1 << 2,
			kManualOverrideStage      = 1 << 3
		};

		//---------------------------------------------------------------------------------------------------------
		// RelationshipEdge
		//
		// A typed directed edge from one asset to another.
		//---------------------------------------------------------------------------------------------------------
		struct RelationshipEdge
		{
			Dia::Core::StringCRC mRelationshipType;  // e.g. "uses", "contains"
			Dia::Core::StringCRC mTargetAssetId;     // e.g. "texture.player_ship"

			RelationshipEdge()
				: mRelationshipType()
				, mTargetAssetId()
			{}

			RelationshipEdge(const Dia::Core::StringCRC& relType, const Dia::Core::StringCRC& targetId)
				: mRelationshipType(relType)
				, mTargetAssetId(targetId)
			{}
		};

		//---------------------------------------------------------------------------------------------------------
		// AssetRecord
		//
		// Canonical metadata record for a single registered asset.
		// ID is a composite "type.name" StringCRC.
		// Source path is stored as a raw string (String256) for simplicity.
		// Forward references are stored inline; reverse references are computed lazily by RelationshipIndex.
		//---------------------------------------------------------------------------------------------------------
		struct AssetRecord
		{
			Dia::Core::StringCRC                                                   mId;            // composite "type.name"
			Dia::Core::StringCRC                                                   mAssetTypeId;   // e.g. "texture"
			Dia::Core::Containers::String256                                       mSourcePath;    // e.g. "Raw/Textures/player_ship.png"
			unsigned int                                                           mContentHash;   // 0 = not yet computed
			AssetStatus                                                            mStatus;
			AssetScope                                                             mScope;
			Dia::Core::StringCRC                                                   mScopeStageName; // only meaningful when mScope == kStage
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>          mTags;
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 8>             mReferences;    // forward refs
			unsigned char                                                          mManualOverrideFlags; // bitmask of ManualOverrideField

			AssetRecord()
				: mId()
				, mAssetTypeId()
				, mSourcePath()
				, mContentHash(0)
				, mStatus(AssetStatus::Active)
				, mScope(AssetScope::kGlobal)
				, mScopeStageName()
				, mTags()
				, mReferences()
				, mManualOverrideFlags(0)
			{}

			bool HasContentHash() const { return mContentHash != 0; }
		};

	} // namespace AssetCatalogue
} // namespace Dia
