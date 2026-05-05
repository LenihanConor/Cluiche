#pragma once

#include "DiaAssetCatalogue/LoadResult.h"
#include "DiaAssetCatalogue/AssetRecord.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetRegistry;
		class AssetTypeRegistry;
		class RelationshipIndex;

		//---------------------------------------------------------------------------------------------------------
		// RuleChange
		//
		// A single proposed mutation from rules evaluation.
		//---------------------------------------------------------------------------------------------------------
		struct RuleChange
		{
			Dia::Core::StringCRC mRecordId;
			Dia::Core::Containers::String64 mField;     // e.g. "scope", "tag", "relationship"
			Dia::Core::Containers::String64 mOldValue;
			Dia::Core::Containers::String64 mNewValue;
			Dia::Core::Containers::String64 mRuleName;
			bool mIsConflict;

			RuleChange()
				: mRecordId()
				, mField()
				, mOldValue()
				, mNewValue()
				, mRuleName()
				, mIsConflict(false)
			{}
		};

		//---------------------------------------------------------------------------------------------------------
		// RuleChangeset
		//
		// Collection of all proposed changes from a rules evaluation pass.
		//---------------------------------------------------------------------------------------------------------
		struct RuleChangeset
		{
			Dia::Core::Containers::DynamicArrayC<RuleChange, 256> mChanges;
			unsigned int mConflictCount;
			bool mTruncated; // true if capacity exceeded

			RuleChangeset()
				: mChanges()
				, mConflictCount(0)
				, mTruncated(false)
			{}
		};

		//---------------------------------------------------------------------------------------------------------
		// CatalogueRulesEngine
		//
		// Loads, validates, and evaluates configurable match-action rules against an AssetRegistry.
		//
		// Rules are evaluated in array order with snapshot semantics:
		//   - Before evaluation, scope and tags are snapshotted
		//   - Each rule's match criteria use the snapshot
		//   - Mutations accumulate in the changeset
		//   - Apply writes final values (last-rule-wins for conflicts)
		//
		// Supports dry-run mode (returns changes without mutation) and conflict detection.
		//---------------------------------------------------------------------------------------------------------
		class CatalogueRulesEngine
		{
		public:
			CatalogueRulesEngine();

			// Load and schema-validate rules file. Returns errors if invalid.
			// Invalid rules files are rejected entirely (not partially loaded).
			LoadResult<void> LoadRules(const char* rulesPath,
				const AssetTypeRegistry& typeRegistry);

			// Dry-run: returns proposed changes without modifying registry.
			RuleChangeset EvaluateDryRun(const AssetRegistry& registry,
				RelationshipIndex& relationships) const;

			// Apply: evaluates rules and mutates registry + relationships.
			// Returns the same changeset that dry-run would have produced.
			RuleChangeset Apply(AssetRegistry& registry,
				RelationshipIndex& relationships) const;

			// Number of loaded rules.
			unsigned int GetRuleCount() const;

		private:
			//-------------------------------------------------------------------
			// Internal rule representation
			//-------------------------------------------------------------------
			enum class MatchType
			{
				Type,             // asset type ID equals value
				SourcePathGlob,   // source path matches glob (simplified substring)
				Tag,              // record already has this tag
				StageRefCount,    // count of Stage references in range [min, max]
				All               // matches every record
			};

			enum class ActionType
			{
				AssignTag,
				AssignScope,
				AssignStage,
				InferReferences,
				ComputeScope
			};

			struct Rule
			{
				Dia::Core::Containers::String64 mName;
				MatchType mMatchType;
				Dia::Core::Containers::String64 mMatchValue;  // type ID, glob pattern, tag, or empty
				unsigned int mStageRefMin;                     // for StageRefCount
				unsigned int mStageRefMax;                     // for StageRefCount
				ActionType mActionType;
				Dia::Core::Containers::String64 mActionParam; // tag, scope, stage_id, or "typed_fields"
				Dia::Core::Containers::String64 mStageName;   // for assign_scope with stage name

				Rule()
					: mName()
					, mMatchType(MatchType::All)
					, mMatchValue()
					, mStageRefMin(0)
					, mStageRefMax(0)
					, mActionType(ActionType::AssignTag)
					, mActionParam()
					, mStageName()
				{}
			};

			//-------------------------------------------------------------------
			// Snapshot for evaluation
			//-------------------------------------------------------------------
			struct RecordSnapshot
			{
				Dia::Core::StringCRC mId;
				AssetScope mScope;
				Dia::Core::StringCRC mScopeStageName;
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> mTags;

				RecordSnapshot()
					: mId()
					, mScope(AssetScope::kGlobal)
					, mScopeStageName()
					, mTags()
				{}
			};

			// Core evaluation logic (shared by dry-run and apply)
			RuleChangeset Evaluate(const AssetRegistry& registry,
				RelationshipIndex& relationships) const;

			// Match a single rule against a record snapshot
			bool MatchesRule(const Rule& rule,
				const RecordSnapshot& snapshot,
				const AssetRecord& record,
				const AssetRegistry& registry,
				RelationshipIndex& relationships) const;

			// Count stage references for an asset
			unsigned int CountStageRefs(const Dia::Core::StringCRC& assetId,
				const AssetRegistry& registry,
				RelationshipIndex& relationships) const;

			static const unsigned int kMaxRules = 64;
			Dia::Core::Containers::DynamicArrayC<Rule, kMaxRules> mRules;
		};

	} // namespace AssetCatalogue
} // namespace Dia
