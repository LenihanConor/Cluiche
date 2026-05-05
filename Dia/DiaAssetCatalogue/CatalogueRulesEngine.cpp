#include "DiaAssetCatalogue/CatalogueRulesEngine.h"
#include "DiaAssetCatalogue/AssetRegistry.h"
#include "DiaAssetCatalogue/AssetTypeRegistry.h"
#include "DiaAssetCatalogue/RelationshipIndex.h"
#include "DiaAssetCatalogue/RelationshipTypes.h"
#include "DiaAssetCatalogue/ScopeComputer.h"
#include "DiaAssetCatalogue/RelationshipInferrer.h"

#include "DiaCore/Json/external/json/json.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String64.h"

#include <stdio.h>
#include <string.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// Constructor
		//------------------------------------------------------------------------------------
		CatalogueRulesEngine::CatalogueRulesEngine()
			: mRules()
		{}

		//------------------------------------------------------------------------------------
		// LoadRules
		//------------------------------------------------------------------------------------
		LoadResult<void> CatalogueRulesEngine::LoadRules(const char* rulesPath,
			const AssetTypeRegistry& typeRegistry)
		{
			LoadResult<void> result;
			mRules.RemoveAll();

			if (rulesPath == nullptr || rulesPath[0] == '\0')
			{
				LoadError err;
				err.mKind = LoadErrorKind::FileNotFound;
				err.mMessage = Dia::Core::Containers::String64("Rules path is null or empty");
				result.mErrors.Add(err);
				return result;
			}

			// Read file
			FILE* f = nullptr;
#if defined(_MSC_VER)
			fopen_s(&f, rulesPath, "rb");
#else
			f = fopen(rulesPath, "rb");
#endif
			if (!f)
			{
				LoadError err;
				err.mKind = LoadErrorKind::FileNotFound;
				err.mMessage = Dia::Core::Containers::String64("Cannot open rules file");
				result.mErrors.Add(err);
				return result;
			}

			fseek(f, 0, SEEK_END);
			long fileSize = ftell(f);
			fseek(f, 0, SEEK_SET);

			static const unsigned int kMaxRulesFileSize = 64 * 1024;
			static char sRulesBuffer[kMaxRulesFileSize];

			if (fileSize <= 0 || static_cast<unsigned long>(fileSize) >= kMaxRulesFileSize)
			{
				fclose(f);
				LoadError err;
				err.mKind = LoadErrorKind::JsonParseError;
				err.mMessage = Dia::Core::Containers::String64("Rules file too large or empty");
				result.mErrors.Add(err);
				return result;
			}

			size_t bytesRead = fread(sRulesBuffer, 1, static_cast<size_t>(fileSize), f);
			fclose(f);
			sRulesBuffer[bytesRead] = '\0';

			// Parse JSON
			Json::Value root;
			Json::Reader reader;
			if (!reader.parse(sRulesBuffer, sRulesBuffer + bytesRead, root))
			{
				LoadError err;
				err.mKind = LoadErrorKind::JsonParseError;
				err.mMessage = Dia::Core::Containers::String64("JSON parse error");
				result.mErrors.Add(err);
				return result;
			}

			if (!root.isObject() || !root.isMember("rules") || !root["rules"].isArray())
			{
				LoadError err;
				err.mKind = LoadErrorKind::MissingRequiredField;
				err.mMessage = Dia::Core::Containers::String64("Missing 'rules' array");
				result.mErrors.Add(err);
				return result;
			}

			const Json::Value& rulesArray = root["rules"];

			// Validate all rules (collect all errors, don't stop at first)
			Dia::Core::Containers::DynamicArrayC<Rule, kMaxRules> parsedRules;

			for (unsigned int i = 0; i < rulesArray.size(); ++i)
			{
				const Json::Value& ruleJson = rulesArray[i];

				if (!ruleJson.isObject())
				{
					LoadError err;
					err.mKind = LoadErrorKind::TypeMismatch;
					err.mMessage = Dia::Core::Containers::String64("Rule entry is not an object");
					result.mErrors.Add(err);
					continue;
				}

				// Validate name
				if (!ruleJson.isMember("name") || !ruleJson["name"].isString())
				{
					LoadError err;
					err.mKind = LoadErrorKind::MissingRequiredField;
					err.mMessage = Dia::Core::Containers::String64("Rule missing 'name'");
					result.mErrors.Add(err);
					continue;
				}

				// Validate match
				if (!ruleJson.isMember("match") || !ruleJson["match"].isObject())
				{
					LoadError err;
					err.mKind = LoadErrorKind::MissingRequiredField;
					err.mMessage = Dia::Core::Containers::String64("Rule missing 'match' object");
					result.mErrors.Add(err);
					continue;
				}

				// Validate action
				if (!ruleJson.isMember("action") || !ruleJson["action"].isString())
				{
					LoadError err;
					err.mKind = LoadErrorKind::MissingRequiredField;
					err.mMessage = Dia::Core::Containers::String64("Rule missing 'action'");
					result.mErrors.Add(err);
					continue;
				}

				Rule rule;
				rule.mName = Dia::Core::Containers::String64(ruleJson["name"].asCString());

				// Parse match criteria (exactly one key)
				const Json::Value& matchJson = ruleJson["match"];
				Json::Value::Members matchKeys = matchJson.getMemberNames();

				if (matchKeys.size() != 1)
				{
					LoadError err;
					err.mKind = LoadErrorKind::TypeMismatch;
					err.mMessage = Dia::Core::Containers::String64("Match must have exactly one key");
					result.mErrors.Add(err);
					continue;
				}

				const std::string& matchKey = matchKeys[0];
				bool matchValid = true;

				if (matchKey == "type")
				{
					rule.mMatchType = MatchType::Type;
					rule.mMatchValue = Dia::Core::Containers::String64(matchJson["type"].asCString());
				}
				else if (matchKey == "source_path_glob")
				{
					rule.mMatchType = MatchType::SourcePathGlob;
					rule.mMatchValue = Dia::Core::Containers::String64(matchJson["source_path_glob"].asCString());
				}
				else if (matchKey == "tag")
				{
					rule.mMatchType = MatchType::Tag;
					rule.mMatchValue = Dia::Core::Containers::String64(matchJson["tag"].asCString());
				}
				else if (matchKey == "stage_ref_count")
				{
					rule.mMatchType = MatchType::StageRefCount;
					const Json::Value& refCount = matchJson["stage_ref_count"];
					if (refCount.isObject())
					{
						rule.mStageRefMin = refCount.get("min", 0).asUInt();
						rule.mStageRefMax = refCount.get("max", 999).asUInt();
					}
					else
					{
						LoadError err;
						err.mKind = LoadErrorKind::TypeMismatch;
						err.mMessage = Dia::Core::Containers::String64("stage_ref_count must be object");
						result.mErrors.Add(err);
						matchValid = false;
					}
				}
				else if (matchKey == "all")
				{
					rule.mMatchType = MatchType::All;
				}
				else
				{
					LoadError err;
					err.mKind = LoadErrorKind::TypeMismatch;
					err.mMessage = Dia::Core::Containers::String64("Unknown match key");
					result.mErrors.Add(err);
					matchValid = false;
				}

				if (!matchValid)
				{
					continue;
				}

				// Parse action type and validate per-action required fields
				const char* actionStr = ruleJson["action"].asCString();
				bool actionValid = true;

				if (strcmp(actionStr, "assign_tag") == 0)
				{
					rule.mActionType = ActionType::AssignTag;
					if (!ruleJson.isMember("tag") || !ruleJson["tag"].isString())
					{
						LoadError err;
						err.mKind = LoadErrorKind::MissingRequiredField;
						err.mMessage = Dia::Core::Containers::String64("assign_tag needs 'tag'");
						result.mErrors.Add(err);
						actionValid = false;
					}
					else
					{
						rule.mActionParam = Dia::Core::Containers::String64(ruleJson["tag"].asCString());
					}
				}
				else if (strcmp(actionStr, "assign_scope") == 0)
				{
					rule.mActionType = ActionType::AssignScope;
					if (!ruleJson.isMember("scope") || !ruleJson["scope"].isString())
					{
						LoadError err;
						err.mKind = LoadErrorKind::MissingRequiredField;
						err.mMessage = Dia::Core::Containers::String64("assign_scope needs 'scope'");
						result.mErrors.Add(err);
						actionValid = false;
					}
					else
					{
						const char* scopeStr = ruleJson["scope"].asCString();
						if (strcmp(scopeStr, "global") != 0 && strcmp(scopeStr, "stage") != 0)
						{
							LoadError err;
							err.mKind = LoadErrorKind::TypeMismatch;
							err.mMessage = Dia::Core::Containers::String64("scope must be global/stage");
							result.mErrors.Add(err);
							actionValid = false;
						}
						else
						{
							rule.mActionParam = Dia::Core::Containers::String64(scopeStr);
							if (ruleJson.isMember("stage_name") && ruleJson["stage_name"].isString())
							{
								rule.mStageName = Dia::Core::Containers::String64(ruleJson["stage_name"].asCString());
							}
						}
					}
				}
				else if (strcmp(actionStr, "assign_stage") == 0)
				{
					rule.mActionType = ActionType::AssignStage;
					if (!ruleJson.isMember("stage_id") || !ruleJson["stage_id"].isString())
					{
						LoadError err;
						err.mKind = LoadErrorKind::MissingRequiredField;
						err.mMessage = Dia::Core::Containers::String64("assign_stage needs 'stage_id'");
						result.mErrors.Add(err);
						actionValid = false;
					}
					else
					{
						const char* stageId = ruleJson["stage_id"].asCString();
						// Validate format: must contain exactly one '.'
						int dotCount = 0;
						int dotPos = -1;
						int sLen = 0;
						for (int c = 0; stageId[c] != '\0'; ++c)
						{
							if (stageId[c] == '.') { ++dotCount; dotPos = c; }
							++sLen;
						}
						if (dotCount != 1 || dotPos == 0 || dotPos == sLen - 1)
						{
							LoadError err;
							err.mKind = LoadErrorKind::TypeMismatch;
							err.mMessage = Dia::Core::Containers::String64("stage_id not type.name format");
							result.mErrors.Add(err);
							actionValid = false;
						}
						else
						{
							rule.mActionParam = Dia::Core::Containers::String64(stageId);
						}
					}
				}
				else if (strcmp(actionStr, "infer_references") == 0)
				{
					rule.mActionType = ActionType::InferReferences;
					if (!ruleJson.isMember("source") || !ruleJson["source"].isString() ||
						strcmp(ruleJson["source"].asCString(), "typed_fields") != 0)
					{
						LoadError err;
						err.mKind = LoadErrorKind::MissingRequiredField;
						err.mMessage = Dia::Core::Containers::String64("infer_references needs source=typed_fields");
						result.mErrors.Add(err);
						actionValid = false;
					}
					else
					{
						rule.mActionParam = Dia::Core::Containers::String64("typed_fields");
					}
				}
				else if (strcmp(actionStr, "compute_scope") == 0)
				{
					rule.mActionType = ActionType::ComputeScope;
				}
				else
				{
					LoadError err;
					err.mKind = LoadErrorKind::TypeMismatch;
					err.mMessage = Dia::Core::Containers::String64("Unknown action type");
					result.mErrors.Add(err);
					actionValid = false;
				}

				if (actionValid)
				{
					if (!parsedRules.IsFull())
					{
						parsedRules.Add(rule);
					}
				}
			}

			// Reject entirely if any errors
			if (result.HasErrors())
			{
				return result;
			}

			// Success — store parsed rules
			for (unsigned int i = 0; i < parsedRules.Size(); ++i)
			{
				mRules.Add(parsedRules[i]);
			}

			result.mSuccess = true;
			return result;
		}

		//------------------------------------------------------------------------------------
		// GetRuleCount
		//------------------------------------------------------------------------------------
		unsigned int CatalogueRulesEngine::GetRuleCount() const
		{
			return mRules.Size();
		}

		//------------------------------------------------------------------------------------
		// EvaluateDryRun
		//------------------------------------------------------------------------------------
		RuleChangeset CatalogueRulesEngine::EvaluateDryRun(const AssetRegistry& registry,
			RelationshipIndex& relationships) const
		{
			return Evaluate(registry, relationships);
		}

		//------------------------------------------------------------------------------------
		// Apply
		//------------------------------------------------------------------------------------
		RuleChangeset CatalogueRulesEngine::Apply(AssetRegistry& registry,
			RelationshipIndex& relationships) const
		{
			RuleChangeset changeset = Evaluate(registry, relationships);

			// Apply changes to the live registry (last-rule-wins)
			for (unsigned int i = 0; i < changeset.mChanges.Size(); ++i)
			{
				const RuleChange& change = changeset.mChanges[i];
				AssetRecord* record = registry.FindById(change.mRecordId);
				if (record == nullptr)
				{
					continue;
				}

				const char* field = change.mField.AsCStr();
				const char* newVal = change.mNewValue.AsCStr();

				if (strcmp(field, "tag") == 0)
				{
					// Add tag if not already present
					Dia::Core::StringCRC tagCrc(newVal);
					bool found = false;
					for (unsigned int t = 0; t < record->mTags.Size(); ++t)
					{
						if (record->mTags[t] == tagCrc)
						{
							found = true;
							break;
						}
					}
					if (!found && !record->mTags.IsFull())
					{
						record->mTags.Add(tagCrc);
					}
				}
				else if (strcmp(field, "scope") == 0)
				{
					if (strcmp(newVal, "global") == 0)
					{
						record->mScope = AssetScope::kGlobal;
						record->mScopeStageName = Dia::Core::StringCRC();
					}
					else if (strcmp(newVal, "stage") == 0)
					{
						record->mScope = AssetScope::kStage;
						// Stage name comes from rule's mStageName or is computed
						// Find matching change to get stage name (it's in the new value as "stage:<name>")
						// Simple approach: look for colon separator
						const char* colon = strchr(newVal, ':');
						if (colon != nullptr)
						{
							record->mScopeStageName = Dia::Core::StringCRC(colon + 1);
						}
					}
				}
				else if (strcmp(field, "relationship") == 0)
				{
					// Add contains relationship from stage to record
					// newVal is the stage ID
					Dia::Core::StringCRC stageId(newVal);
					AssetRecord* stageRecord = registry.FindById(stageId);
					if (stageRecord != nullptr)
					{
						// Add contains edge from stage to this record
						RelationshipEdge edge(RelationshipTypes::kContains, change.mRecordId);
						if (!stageRecord->mReferences.IsFull())
						{
							stageRecord->mReferences.Add(edge);
						}
						relationships.InvalidateReverseCache();
					}
				}
			}

			return changeset;
		}

		//------------------------------------------------------------------------------------
		// Evaluate (core logic shared by dry-run and apply)
		//------------------------------------------------------------------------------------
		RuleChangeset CatalogueRulesEngine::Evaluate(const AssetRegistry& registry,
			RelationshipIndex& relationships) const
		{
			RuleChangeset changeset;

			if (mRules.Size() == 0 || registry.GetCount() == 0)
			{
				return changeset;
			}

			// Take snapshot of all records' scope and tags
			static const unsigned int kMaxSnapshots = 128;
			Dia::Core::Containers::DynamicArrayC<RecordSnapshot, kMaxSnapshots> snapshots;

			for (unsigned int i = 0; i < registry.GetCount(); ++i)
			{
				const AssetRecord& record = registry.GetRecordByIndex(i);
				RecordSnapshot snap;
				snap.mId = record.mId;
				snap.mScope = record.mScope;
				snap.mScopeStageName = record.mScopeStageName;
				for (unsigned int t = 0; t < record.mTags.Size(); ++t)
				{
					snap.mTags.Add(record.mTags[t]);
				}
				if (!snapshots.IsFull())
				{
					snapshots.Add(snap);
				}
			}

			// Evaluate each rule in order
			for (unsigned int ruleIdx = 0; ruleIdx < mRules.Size(); ++ruleIdx)
			{
				const Rule& rule = mRules[ruleIdx];

				for (unsigned int recIdx = 0; recIdx < registry.GetCount(); ++recIdx)
				{
					const AssetRecord& record = registry.GetRecordByIndex(recIdx);

					// Find snapshot for this record
					const RecordSnapshot* snapshot = nullptr;
					for (unsigned int s = 0; s < snapshots.Size(); ++s)
					{
						if (snapshots[s].mId == record.mId)
						{
							snapshot = &snapshots[s];
							break;
						}
					}

					if (snapshot == nullptr)
					{
						continue;
					}

					// Check if rule matches this record (use snapshot for matching)
					if (!MatchesRule(rule, *snapshot, record, registry, relationships))
					{
						continue;
					}

					// Rule matched — produce change(s) based on action type
					if (changeset.mChanges.IsFull())
					{
						changeset.mTruncated = true;
						return changeset;
					}

					switch (rule.mActionType)
					{
					case ActionType::AssignTag:
					{
						// Check if tag is already present in snapshot
						Dia::Core::StringCRC tagCrc(rule.mActionParam.AsCStr());
						bool alreadyHas = false;
						for (unsigned int t = 0; t < snapshot->mTags.Size(); ++t)
						{
							if (snapshot->mTags[t] == tagCrc)
							{
								alreadyHas = true;
								break;
							}
						}
						if (!alreadyHas)
						{
							RuleChange change;
							change.mRecordId = record.mId;
							change.mField = Dia::Core::Containers::String64("tag");
							change.mOldValue = Dia::Core::Containers::String64("");
							change.mNewValue = rule.mActionParam;
							change.mRuleName = rule.mName;
							change.mIsConflict = false;
							changeset.mChanges.Add(change);
						}
						break;
					}
					case ActionType::AssignScope:
					{
						const char* newScope = rule.mActionParam.AsCStr();
						const char* oldScope = (snapshot->mScope == AssetScope::kGlobal) ? "global" : "stage";

						if (strcmp(newScope, oldScope) != 0)
						{
							RuleChange change;
							change.mRecordId = record.mId;
							change.mField = Dia::Core::Containers::String64("scope");
							change.mOldValue = Dia::Core::Containers::String64(oldScope);
							change.mNewValue = rule.mActionParam;
							change.mRuleName = rule.mName;
							change.mIsConflict = false;
							changeset.mChanges.Add(change);
						}
						break;
					}
					case ActionType::AssignStage:
					{
						// Add a "contains" relationship from the named stage to this record
						RuleChange change;
						change.mRecordId = record.mId;
						change.mField = Dia::Core::Containers::String64("relationship");
						change.mOldValue = Dia::Core::Containers::String64("");
						change.mNewValue = rule.mActionParam;
						change.mRuleName = rule.mName;
						change.mIsConflict = false;
						changeset.mChanges.Add(change);
						break;
					}
					case ActionType::InferReferences:
					{
						// Note: infer_references requires TypeInstance which we don't have from registry alone
						// This is a no-op in the basic rules engine evaluation
						// The editor would need to provide loaded instances
						break;
					}
					case ActionType::ComputeScope:
					{
						// Skip stage records
						static const Dia::Core::StringCRC kStageTypeId("stage");
						if (record.mAssetTypeId == kStageTypeId)
						{
							break;
						}

						ScopeComputer computer;
						Dia::Core::StringCRC outStageName;
						AssetScope newScope = computer.ComputeScope(record.mId, registry, relationships, outStageName);

						const char* newScopeStr = (newScope == AssetScope::kGlobal) ? "global" : "stage";
						const char* oldScopeStr = (snapshot->mScope == AssetScope::kGlobal) ? "global" : "stage";

						if (strcmp(newScopeStr, oldScopeStr) != 0)
						{
							RuleChange change;
							change.mRecordId = record.mId;
							change.mField = Dia::Core::Containers::String64("scope");
							change.mOldValue = Dia::Core::Containers::String64(oldScopeStr);
							change.mNewValue = Dia::Core::Containers::String64(newScopeStr);
							change.mRuleName = rule.mName;
							change.mIsConflict = false;
							changeset.mChanges.Add(change);
						}
						break;
					}
					}
				}
			}

			// Conflict detection: find cases where two rules assign different values
			// to the same field on the same record
			for (unsigned int i = 0; i < changeset.mChanges.Size(); ++i)
			{
				for (unsigned int j = i + 1; j < changeset.mChanges.Size(); ++j)
				{
					RuleChange& a = changeset.mChanges[i];
					RuleChange& b = changeset.mChanges[j];

					if (a.mRecordId == b.mRecordId &&
						strcmp(a.mField.AsCStr(), b.mField.AsCStr()) == 0 &&
						strcmp(a.mField.AsCStr(), "tag") != 0) // tags are additive, not conflicting
					{
						// Same record, same field, different rules
						if (strcmp(a.mRuleName.AsCStr(), b.mRuleName.AsCStr()) != 0 &&
							strcmp(a.mNewValue.AsCStr(), b.mNewValue.AsCStr()) != 0)
						{
							if (!a.mIsConflict)
							{
								a.mIsConflict = true;
								++changeset.mConflictCount;
							}
							if (!b.mIsConflict)
							{
								b.mIsConflict = true;
								++changeset.mConflictCount;
							}
						}
					}
				}
			}

			return changeset;
		}

		//------------------------------------------------------------------------------------
		// MatchesRule
		//------------------------------------------------------------------------------------
		bool CatalogueRulesEngine::MatchesRule(const Rule& rule,
			const RecordSnapshot& snapshot,
			const AssetRecord& record,
			const AssetRegistry& registry,
			RelationshipIndex& relationships) const
		{
			switch (rule.mMatchType)
			{
			case MatchType::All:
				return true;

			case MatchType::Type:
			{
				Dia::Core::StringCRC matchType(rule.mMatchValue.AsCStr());
				return record.mAssetTypeId == matchType;
			}

			case MatchType::SourcePathGlob:
			{
				// Simplified glob matching: strip * and ** from pattern, check if path contains
				// the remaining segments
				const char* pattern = rule.mMatchValue.AsCStr();
				const char* path = record.mSourcePath.AsCStr();

				// Extract non-wildcard segments and check if path contains them
				// Simple approach: remove leading/trailing * and **, check substring
				char stripped[64];
				int stripIdx = 0;
				for (int p = 0; pattern[p] != '\0' && stripIdx < 63; ++p)
				{
					if (pattern[p] == '*')
					{
						continue; // skip wildcards
					}
					stripped[stripIdx++] = pattern[p];
				}
				stripped[stripIdx] = '\0';

				// Check if path contains the stripped pattern
				if (stripIdx == 0)
				{
					return true; // all wildcards = match everything
				}

				return strstr(path, stripped) != nullptr;
			}

			case MatchType::Tag:
			{
				// Use snapshot tags for matching
				Dia::Core::StringCRC matchTag(rule.mMatchValue.AsCStr());
				for (unsigned int t = 0; t < snapshot.mTags.Size(); ++t)
				{
					if (snapshot.mTags[t] == matchTag)
					{
						return true;
					}
				}
				return false;
			}

			case MatchType::StageRefCount:
			{
				unsigned int count = CountStageRefs(record.mId, registry, relationships);
				return count >= rule.mStageRefMin && count <= rule.mStageRefMax;
			}
			}

			return false;
		}

		//------------------------------------------------------------------------------------
		// CountStageRefs
		//------------------------------------------------------------------------------------
		unsigned int CatalogueRulesEngine::CountStageRefs(const Dia::Core::StringCRC& assetId,
			const AssetRegistry& registry,
			RelationshipIndex& relationships) const
		{
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> containingIds;
			relationships.GetReverseRefsByType(assetId, RelationshipTypes::kContains, registry, containingIds);

			unsigned int stageCount = 0;
			static const Dia::Core::StringCRC kStageTypeId("stage");

			for (unsigned int i = 0; i < containingIds.Size(); ++i)
			{
				const AssetRecord* containerRecord = registry.FindById(containingIds[i]);
				if (containerRecord != nullptr && containerRecord->mAssetTypeId == kStageTypeId)
				{
					++stageCount;
				}
			}

			return stageCount;
		}

	} // namespace AssetCatalogue
} // namespace Dia
