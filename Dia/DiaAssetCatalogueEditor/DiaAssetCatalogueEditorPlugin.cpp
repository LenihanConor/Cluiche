#include "DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h"
#include "DiaAssetCatalogueEditor/Commands/CreateRecordCommand.h"
#include "DiaAssetCatalogueEditor/Commands/UpdateRecordCommand.h"
#include "DiaAssetCatalogueEditor/Commands/DeleteRecordCommand.h"
#include "DiaAssetCatalogueEditor/Handlers/FileDiscoverer.h"
#include "DiaAssetCatalogueEditor/Commands/AddRelationshipCommand.h"
#include "DiaAssetCatalogueEditor/Commands/RemoveRelationshipCommand.h"
#include "DiaAssetCatalogueEditor/Handlers/AssetTypeEditorRegistry.h"
#include "DiaAssetCatalogueEditor/Commands/ApplyRulesCommand.h"
#include <DiaEditor/Plugin/IPluginLoader.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>

#include <cstring>

// Output dir for session context per SED-020 / SD-ACE-005.
// Real path resolved at runtime from RepoRoot; fall back to relative path.
static const char* kDefaultOutputDir = "Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor";

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			void DiaAssetCatalogueEditorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
			{
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnLoad");

				mBridge       = context.mBridge;
				mView         = context.mView;
				mPluginLoader = context.mPluginLoader;

				strncpy_s(mOutputDir, kOutputDirLength, kDefaultOutputDir, _TRUNCATE);
				mCurrentPath[0] = '\0';

				mSessionContext.Load(mOutputDir);

				RegisterRequestHandlers();

				// Offer re-open if session context has a last path
				if (mSessionContext.HasLastManifestPath())
				{
					Json::Value msg;
					msg["lastManifestPath"] = mSessionContext.GetLastManifestPath();
					if (mBridge)
						mBridge->NotifyUIDataChanged("assetcatalogue.lastManifestPath", msg);
				}

				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: Initialized");
			}

			void DiaAssetCatalogueEditorPlugin::OnUnload()
			{
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnUnload");

				if (mBridge)
				{
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.load_manifest"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.save_manifest"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.new_manifest"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.bulk_create_records"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.discover_files"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.add_relationship"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.remove_relationship"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.get_forward_refs"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.get_reverse_refs"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.validate"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.open_asset"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.register_type_editor"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.load_rules"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.get_rules"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.dry_run_rules"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.apply_rules"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.query_asset_ids"));
				}

				mSessionContext.Save(mOutputDir);
				mBridge       = nullptr;
				mView         = nullptr;
				mPluginLoader = nullptr;
			}

			void DiaAssetCatalogueEditorPlugin::OnUpdate(float /*deltaTime*/)
			{
			}

			void DiaAssetCatalogueEditorPlugin::RegisterRequestHandlers()
			{
				if (!mBridge)
					return;

				RegisterCRUDHandlers();
				RegisterDiscovererHandlers();
				RegisterRelationshipHandlers();
				RegisterValidationHandlers();
				RegisterAssetTypeEditorHandlers();
				RegisterRulesHandlers();

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.load_manifest"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("path") || !data["path"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing path parameter";
							return result;
						}
						const char* path = data["path"].asCString();

						char errorBuf[256] = {};
						bool ok = mLoadHandler.Load(path, mRegistry, mSerializer, mHistory,
							errorBuf, sizeof(errorBuf));

						if (ok)
						{
							strncpy_s(mCurrentPath, kCurrentPathLength, path, _TRUNCATE);
							mSessionContext.SetLastManifestPath(path);
							mSessionContext.Save(mOutputDir);

							result["success"]      = true;
							result["record_count"] = static_cast<int>(mRegistry.GetCount());
							PushDirtyState();
						}
						else
						{
							result["success"] = false;
							result["error"]   = errorBuf[0] ? errorBuf : "load failed";
						}
						return result;
					});

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.save_manifest"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;

						// Use provided path, or fall back to current path
						const char* path = mCurrentPath;
						if (data.isMember("path") && data["path"].isString())
							path = data["path"].asCString();

						if (!path || path[0] == '\0')
						{
							result["success"] = false;
							result["error"]   = "no path specified";
							return result;
						}

						char errorBuf[256] = {};
						bool ok = mLoadHandler.Save(path, mRegistry, mSerializer, mHistory,
							errorBuf, sizeof(errorBuf));

						if (ok)
						{
							strncpy_s(mCurrentPath, kCurrentPathLength, path, _TRUNCATE);
							mSessionContext.SetLastManifestPath(path);
							mSessionContext.Save(mOutputDir);
							result["success"] = true;
							PushDirtyState();
						}
						else
						{
							result["success"] = false;
							result["error"]   = errorBuf[0] ? errorBuf : "save failed";
						}
						return result;
					});

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.new_manifest"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						mLoadHandler.NewManifest(mRegistry, mHistory);
						mCurrentPath[0] = '\0';
						PushDirtyState();

						Json::Value result;
						result["success"] = true;
						return result;
					});
			}

			void DiaAssetCatalogueEditorPlugin::PushDirtyState()
			{
				if (!mBridge)
					return;
				Json::Value data;
				data["dirty"] = mLoadHandler.IsDirty(mHistory);
				data["path"]  = mCurrentPath;
				mBridge->NotifyUIDataChanged("assetcatalogue.state", data);
			}

			void DiaAssetCatalogueEditorPlugin::PushRegistryState()
			{
				if (!mBridge)
					return;
				Json::Value records(Json::arrayValue);
				for (unsigned int i = 0; i < mRegistry.GetCount(); ++i)
					records.append(RecordToJson(mRegistry.GetRecordByIndex(i)));
				mBridge->NotifyUIDataChanged("assetcatalogue.records", records);
				PushDirtyState();
			}

			Dia::AssetCatalogue::AssetRecord DiaAssetCatalogueEditorPlugin::RecordFromJson(const Json::Value& d)
			{
				using namespace Dia::AssetCatalogue;
				AssetRecord rec;
				if (d.isMember("id") && d["id"].isString())
					rec.mId = Dia::Core::StringCRC(d["id"].asCString());
				if (d.isMember("type") && d["type"].isString())
					rec.mAssetTypeId = Dia::Core::StringCRC(d["type"].asCString());
				if (d.isMember("source_path") && d["source_path"].isString())
					rec.mSourcePath = d["source_path"].asCString();
				if (d.isMember("status") && d["status"].isString())
				{
					const char* s = d["status"].asCString();
					if (strcmp(s, "Draft") == 0)            rec.mStatus = AssetStatus::Draft;
					else if (strcmp(s, "Deprecated") == 0)  rec.mStatus = AssetStatus::Deprecated;
					else                                    rec.mStatus = AssetStatus::Active;
				}
				if (d.isMember("scope") && d["scope"].isString())
				{
					if (strcmp(d["scope"].asCString(), "stage") == 0)
					{
						rec.mScope = AssetScope::kStage;
						if (d.isMember("stage") && d["stage"].isString())
							rec.mScopeStageName = Dia::Core::StringCRC(d["stage"].asCString());
					}
					else
					{
						rec.mScope = AssetScope::kGlobal;
					}
				}
				if (d.isMember("tags") && d["tags"].isArray())
				{
					for (const Json::Value& tag : d["tags"])
					{
						if (tag.isString() && !rec.mTags.IsFull())
							rec.mTags.Add(Dia::Core::StringCRC(tag.asCString()));
					}
				}
				return rec;
			}

			Json::Value DiaAssetCatalogueEditorPlugin::RecordToJson(const Dia::AssetCatalogue::AssetRecord& rec)
			{
				using namespace Dia::AssetCatalogue;
				Json::Value d;
				d["id"]          = rec.mId.AsChar();
				d["type"]        = rec.mAssetTypeId.AsChar();
				d["source_path"] = rec.mSourcePath.AsCStr();
				d["content_hash"] = static_cast<Json::UInt>(rec.mContentHash);

				switch (rec.mStatus)
				{
					case AssetStatus::Draft:      d["status"] = "Draft"; break;
					case AssetStatus::Deprecated: d["status"] = "Deprecated"; break;
					default:                      d["status"] = "Active"; break;
				}

				d["scope"] = (rec.mScope == AssetScope::kStage) ? "stage" : "global";
				if (rec.mScope == AssetScope::kStage)
					d["stage"] = rec.mScopeStageName.AsChar();

				Json::Value tags(Json::arrayValue);
				for (unsigned int i = 0; i < rec.mTags.Size(); ++i)
					tags.append(rec.mTags[i].AsChar());
				d["tags"] = tags;

				Json::Value refs(Json::arrayValue);
				for (unsigned int i = 0; i < rec.mReferences.Size(); ++i)
				{
					Json::Value edge;
					edge["rel"]    = rec.mReferences[i].mRelationshipType.AsChar();
					edge["target"] = rec.mReferences[i].mTargetAssetId.AsChar();
					refs.append(edge);
				}
				d["references"] = refs;
				return d;
			}

			// relationship handlers
			void DiaAssetCatalogueEditorPlugin::RegisterRelationshipHandlers()
			{
				if (!mBridge)
					return;

				// add_relationship
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.add_relationship"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("from") || !data.isMember("rel") || !data.isMember("to"))
						{
							result["success"] = false;
							result["error"]   = "missing from/rel/to";
							return result;
						}
						Dia::Core::StringCRC fromId(data["from"].asCString());
						Dia::Core::StringCRC relType(data["rel"].asCString());
						Dia::Core::StringCRC toId(data["to"].asCString());

						if (fromId == toId)
						{
							result["success"] = false;
							result["error"]   = "self-referential edge not allowed";
							return result;
						}

						const Dia::AssetCatalogue::AssetRecord* fromRec = mRegistry.FindById(fromId);
						if (!fromRec || !mRegistry.FindById(toId))
						{
							result["success"] = false;
							result["error"]   = "record not found";
							return result;
						}

						for (unsigned int i = 0; i < fromRec->mReferences.Size(); ++i)
						{
							if (fromRec->mReferences[i].mRelationshipType == relType &&
								fromRec->mReferences[i].mTargetAssetId == toId)
							{
								result["success"] = false;
								result["error"]   = "duplicate edge";
								return result;
							}
						}

						auto* cmd = new Dia::AssetCatalogue::Editor::AddRelationshipCommand(
							mRegistry, fromId, relType, toId);
						mHistory.ExecuteCommand(cmd);

						result["success"] = true;
						PushRegistryState();
						return result;
					});

				// remove_relationship
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.remove_relationship"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("from") || !data.isMember("rel") || !data.isMember("to"))
						{
							result["success"] = false;
							result["error"]   = "missing from/rel/to";
							return result;
						}
						Dia::Core::StringCRC fromId(data["from"].asCString());
						Dia::Core::StringCRC relType(data["rel"].asCString());
						Dia::Core::StringCRC toId(data["to"].asCString());

						auto* cmd = new Dia::AssetCatalogue::Editor::RemoveRelationshipCommand(
							mRegistry, fromId, relType, toId);
						mHistory.ExecuteCommand(cmd);

						result["success"] = true;
						PushRegistryState();
						return result;
					});

				// get_forward_refs (read-only query)
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.get_forward_refs"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("id") || !data["id"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing id";
							return result;
						}
						Dia::Core::StringCRC assetId(data["id"].asCString());

						Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16> refs;
						mRegistry.GetRelationshipIndex().GetForwardRefs(assetId, mRegistry, refs);

						Json::Value arr(Json::arrayValue);
						for (unsigned int i = 0; i < refs.Size(); ++i)
						{
							Json::Value edge;
							edge["rel"]    = refs[i].mRelationshipType.AsChar();
							edge["target"] = refs[i].mTargetAssetId.AsChar();
							arr.append(edge);
						}
						result["success"] = true;
						result["refs"]    = arr;
						return result;
					});

				// get_reverse_refs (read-only query)
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.get_reverse_refs"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("id") || !data["id"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing id";
							return result;
						}
						Dia::Core::StringCRC assetId(data["id"].asCString());

						Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16> refs;
						mRegistry.GetRelationshipIndex().GetReverseRefs(assetId, mRegistry, refs);

						Json::Value arr(Json::arrayValue);
						for (unsigned int i = 0; i < refs.Size(); ++i)
						{
							Json::Value edge;
							edge["rel"]    = refs[i].mRelationshipType.AsChar();
							edge["source"] = refs[i].mTargetAssetId.AsChar(); // mTargetAssetId holds the "from" in reverse results
							arr.append(edge);
						}
						result["success"] = true;
						result["refs"]    = arr;
						return result;
					});
			}

			// discover_files handler
			void DiaAssetCatalogueEditorPlugin::RegisterDiscovererHandlers()
			{
				if (!mBridge)
					return;

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.discover_files"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("root_path") || !data["root_path"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing root_path";
							return result;
						}

						const char* rootPath = data["root_path"].asCString();

						Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles> discovered;
						mFileDiscoverer.Discover(rootPath, mTypeRegistry, mRegistry, discovered);

						Json::Value files(Json::arrayValue);
						for (unsigned int i = 0; i < discovered.Size(); ++i)
						{
							const DiscoveredFile& df = discovered[i];
							Json::Value entry;
							entry["path"]           = df.mFullPath;
							entry["suggested_type"] = df.mSuggestedType;
							entry["suggested_id"]   = df.mSuggestedId;
							entry["file_size"]      = static_cast<Json::UInt64>(df.mFileSize);
							entry["last_modified"]  = static_cast<Json::UInt64>(df.mLastModified);
							files.append(entry);
						}

						result["success"] = true;
						result["files"]   = files;
						return result;
					});
			}

			// rules handlers
			void DiaAssetCatalogueEditorPlugin::RegisterRulesHandlers()
			{
				if (!mBridge)
					return;

				// load_rules
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.load_rules"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("path") || !data["path"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing path";
							return result;
						}
						const char* rulesPath = data["path"].asCString();
						Dia::AssetCatalogue::LoadResult<void> lr = mRulesEngine.LoadRules(rulesPath, mTypeRegistry);
						if (!lr.mSuccess)
						{
							result["success"] = false;
							result["error"]   = lr.HasErrors() ? lr.GetFirstError().mMessage.AsCStr() : "load failed";
						}
						else
						{
							result["success"]    = true;
							result["rule_count"] = static_cast<int>(mRulesEngine.GetRuleCount());
						}
						return result;
					});

				// dry_run_rules
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.dry_run_rules"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value result;
						Dia::AssetCatalogue::RuleChangeset changeset =
							mRulesEngine.EvaluateDryRun(mRegistry, mRegistry.GetRelationshipIndex());

						Json::Value changes(Json::arrayValue);
						for (unsigned int i = 0; i < changeset.mChanges.Size(); ++i)
						{
							const Dia::AssetCatalogue::RuleChange& c = changeset.mChanges[i];
							Json::Value item;
							item["recordId"]  = c.mRecordId.AsChar();
							item["field"]     = c.mField.AsCStr();
							item["oldValue"]  = c.mOldValue.AsCStr();
							item["newValue"]  = c.mNewValue.AsCStr();
							item["ruleName"]  = c.mRuleName.AsCStr();
							item["conflict"]  = c.mIsConflict;
							item["manualOverride"] = c.mIsManualOverride;
							changes.append(item);
						}
						result["success"]       = true;
						result["changes"]       = changes;
						result["conflict_count"] = static_cast<int>(changeset.mConflictCount);
						result["truncated"]     = changeset.mTruncated;
						return result;
					});

				// apply_rules
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.apply_rules"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						auto* cmd = new Dia::AssetCatalogue::Editor::ApplyRulesCommand(
							mRegistry, mRegistry.GetRelationshipIndex(), mRulesEngine);

						if (data.isMember("excluded") && data["excluded"].isArray())
						{
							for (unsigned int i = 0; i < data["excluded"].size(); ++i)
							{
								if (data["excluded"][i].isString())
									cmd->AddExcludedId(Dia::Core::StringCRC(data["excluded"][i].asCString()));
							}
						}

						if (data.isMember("overwrite_manuals") && data["overwrite_manuals"].isBool())
							cmd->SetOverwriteManuals(data["overwrite_manuals"].asBool());

						mHistory.ExecuteCommand(cmd);

						const Dia::AssetCatalogue::RuleChangeset& cs = cmd->GetChangeset();
						result["success"]       = true;
						result["applied_count"] = static_cast<int>(cs.mChanges.Size());
						PushRegistryState();
						return result;
					});

				// get_rules — returns loaded rules list for UI display
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.get_rules"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value result;
						Json::Value rules(Json::arrayValue);
						for (unsigned int i = 0; i < mRulesEngine.GetRuleCount(); ++i)
						{
							Dia::AssetCatalogue::RuleInfo info = mRulesEngine.GetRule(i);
							Json::Value rule;
							rule["name"]        = info.mName.AsCStr();
							rule["match"]       = info.mMatchType.AsCStr();
							rule["matchValue"]  = info.mMatchValue.AsCStr();
							rule["action"]      = info.mActionType.AsCStr();
							rule["actionParam"] = info.mActionParam.AsCStr();
							rules.append(rule);
						}
						result["success"] = true;
						result["rules"]   = rules;
						return result;
					});

				// query_asset_ids — returns filtered list of registry IDs for autocomplete
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.query_asset_ids"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						const char* prefix = nullptr;
						unsigned int prefixLen = 0;
						if (data.isMember("prefix") && data["prefix"].isString())
						{
							prefix = data["prefix"].asCString();
							prefixLen = static_cast<unsigned int>(strlen(prefix));
						}

						Dia::Core::StringCRC filterType;
						if (data.isMember("typeId") && data["typeId"].isString())
							filterType = Dia::Core::StringCRC(data["typeId"].asCString());

						static const unsigned int kMaxAutocompleteResults = 10;
						Json::Value ids(Json::arrayValue);
						for (unsigned int i = 0; i < mRegistry.GetCount(); ++i)
						{
							const Dia::AssetCatalogue::AssetRecord& rec = mRegistry.GetRecordByIndex(i);

							if (filterType != Dia::Core::StringCRC() && rec.mAssetTypeId != filterType)
								continue;

							const char* idStr = rec.mId.AsChar();
							if (prefix && prefixLen > 0 && strncmp(idStr, prefix, prefixLen) != 0)
								continue;

							ids.append(idStr);
							if (ids.size() >= kMaxAutocompleteResults)
								break;
						}
						result["success"] = true;
						result["ids"]     = ids;
						return result;
					});
			}

			// asset type editor routing handlers
			void DiaAssetCatalogueEditorPlugin::RegisterAssetTypeEditorHandlers()
			{
				if (!mBridge)
					return;

				// register_type_editor — allows other plugins to bind a type to an editor
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.register_type_editor"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("assetType") || !data.isMember("editorPluginType"))
						{
							result["success"] = false;
							result["error"]   = "missing assetType or editorPluginType";
							return result;
						}
						Dia::Core::StringCRC assetType(data["assetType"].asCString());
						Dia::Core::StringCRC editorType(data["editorPluginType"].asCString());
						mTypeEditorRegistry.RegisterTypeEditor(assetType, editorType);
						result["success"] = true;
						return result;
					});

				// open_asset — open in registered editor, or fall back to ShellExecuteExW
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.open_asset"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("id") || !data["id"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing id";
							return result;
						}
						Dia::Core::StringCRC assetId(data["id"].asCString());
						const Dia::AssetCatalogue::AssetRecord* rec = mRegistry.FindById(assetId);
						if (!rec)
						{
							result["success"] = false;
							result["error"]   = "record not found";
							return result;
						}

						// Check for a registered editor plugin
						Dia::Core::StringCRC editorPluginType = mTypeEditorRegistry.FindEditorForType(rec->mAssetTypeId);
						if (editorPluginType != Dia::Core::StringCRC() && mPluginLoader)
						{
							mPluginLoader->LoadPlugin(editorPluginType, assetId);
							result["success"] = true;
							result["method"]  = "plugin";
							return result;
						}

						// Fall back to OS shell open
						if (!rec->mSourcePath.IsEmpty())
						{
							wchar_t wPath[512] = {};
							MultiByteToWideChar(CP_UTF8, 0, rec->mSourcePath.AsCStr(), -1, wPath, 512);

							SHELLEXECUTEINFOW sei = {};
							sei.cbSize = sizeof(sei);
							sei.fMask  = SEE_MASK_DEFAULT;
							sei.lpVerb = L"open";
							sei.lpFile = wPath;
							sei.nShow  = SW_SHOWNORMAL;
							ShellExecuteExW(&sei);

							result["success"] = true;
							result["method"]  = "shell";
						}
						else
						{
							DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: open_asset — no source path and no editor registered for type");
							result["success"] = false;
							result["error"]   = "no source path and no registered editor for this type";
						}
						return result;
					});
			}

			// validate handler
			void DiaAssetCatalogueEditorPlugin::RegisterValidationHandlers()
			{
				if (!mBridge)
					return;

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.validate"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value errors(Json::arrayValue);

						for (unsigned int i = 0; i < mRegistry.GetCount(); ++i)
						{
							const Dia::AssetCatalogue::AssetRecord& rec = mRegistry.GetRecordByIndex(i);
							const char* id = rec.mId.AsChar();

							// Missing source path for non-draft assets
							if (rec.mStatus != Dia::AssetCatalogue::AssetStatus::Draft &&
							    rec.mSourcePath.IsEmpty())
							{
								Json::Value e;
								e["assetId"]  = id;
								e["severity"] = "error";
								e["type"]     = "missing_source_path";
								e["message"]  = "Non-draft asset has no source path";
								errors.append(e);
							}

							// Stage-scoped asset missing stage name
							if (rec.mScope == Dia::AssetCatalogue::AssetScope::kStage &&
							    rec.mScopeStageName == Dia::Core::StringCRC())
							{
								Json::Value e;
								e["assetId"]  = id;
								e["severity"] = "error";
								e["type"]     = "missing_stage_name";
								e["message"]  = "Stage-scoped asset has no stage name";
								errors.append(e);
							}

							// Dangling references (target not in registry)
							for (unsigned int r = 0; r < rec.mReferences.Size(); ++r)
							{
								const Dia::Core::StringCRC& targetId = rec.mReferences[r].mTargetAssetId;
								if (!mRegistry.FindById(targetId))
								{
									Json::Value e;
									e["assetId"]  = id;
									e["severity"] = "error";
									e["type"]     = "dangling_reference";
									char msg[320];
									_snprintf_s(msg, sizeof(msg), _TRUNCATE,
										"References unknown asset: %s", targetId.AsChar());
									e["message"]  = msg;
									errors.append(e);
								}
							}

							// Warn on zero content hash (not yet computed)
							if (rec.mContentHash == 0 && !rec.mSourcePath.IsEmpty())
							{
								Json::Value e;
								e["assetId"]  = id;
								e["severity"] = "warning";
								e["type"]     = "missing_content_hash";
								e["message"]  = "Content hash not computed";
								errors.append(e);
							}
						}

						Json::Value result;
						result["success"] = true;
						result["errors"]  = errors;
						return result;
					});
			}

			// CRUD request handlers — appended to RegisterRequestHandlers
			void DiaAssetCatalogueEditorPlugin::RegisterCRUDHandlers()
			{
				if (!mBridge)
					return;

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.create_record"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						Dia::AssetCatalogue::AssetRecord rec = RecordFromJson(data);
						if (rec.mId == Dia::Core::StringCRC())
						{
							result["success"] = false;
							result["error"]   = "missing id";
							return result;
						}

						// Compute content hash
						if (!rec.mSourcePath.IsEmpty())
							rec.mContentHash = mContentHasher.ComputeHash(rec.mSourcePath.AsCStr());

						auto* cmd = new Dia::AssetCatalogue::Editor::CreateRecordCommand(mRegistry, rec);
						mHistory.ExecuteCommand(cmd);

						result["success"]      = true;
						result["content_hash"] = static_cast<Json::UInt>(rec.mContentHash);
						PushRegistryState();
						return result;
					});

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.update_record"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("id") || !data["id"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing id";
							return result;
						}
						Dia::Core::StringCRC recordId(data["id"].asCString());
						const Dia::AssetCatalogue::AssetRecord* existing = mRegistry.FindById(recordId);
						if (!existing)
						{
							result["success"] = false;
							result["error"]   = "record not found";
							return result;
						}

						Dia::AssetCatalogue::AssetRecord newRec = RecordFromJson(data);
						newRec.mId = recordId; // ID is immutable

						// Recompute hash if source path changed
						bool pathChanged = !(newRec.mSourcePath == existing->mSourcePath);
						if (pathChanged && !newRec.mSourcePath.IsEmpty())
							newRec.mContentHash = mContentHasher.ComputeHash(newRec.mSourcePath.AsCStr());
						else
							newRec.mContentHash = existing->mContentHash;

						auto* cmd = new Dia::AssetCatalogue::Editor::UpdateRecordCommand(
							mRegistry, recordId, *existing, newRec);
						mHistory.ExecuteCommand(cmd);

						result["success"]      = true;
						result["content_hash"] = static_cast<Json::UInt>(newRec.mContentHash);
						PushRegistryState();
						return result;
					});

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.delete_record"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("id") || !data["id"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing id";
							return result;
						}
						Dia::Core::StringCRC recordId(data["id"].asCString());
						if (!mRegistry.FindById(recordId))
						{
							result["success"] = false;
							result["error"]   = "record not found";
							return result;
						}

						auto* cmd = new Dia::AssetCatalogue::Editor::DeleteRecordCommand(
							mRegistry, mRegistry.GetRelationshipIndex(), recordId);
						mHistory.ExecuteCommand(cmd);

						result["success"] = true;
						PushRegistryState();
						return result;
					});

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.query_by_type"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("typeId") || !data["typeId"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing typeId";
							return result;
						}
						Dia::Core::StringCRC typeId(data["typeId"].asCString());
						Dia::Core::Containers::DynamicArrayC<const Dia::AssetCatalogue::AssetRecord*, 64> records;
						mRegistry.QueryByType(typeId, records);

						Json::Value arr(Json::arrayValue);
						for (unsigned int i = 0; i < records.Size(); ++i)
							arr.append(RecordToJson(*records[i]));
						result["success"] = true;
						result["records"] = arr;
						return result;
					});

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.query_by_tag"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("tag") || !data["tag"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing tag";
							return result;
						}
						Dia::Core::StringCRC tag(data["tag"].asCString());
						Dia::Core::Containers::DynamicArrayC<const Dia::AssetCatalogue::AssetRecord*, 64> records;
						mRegistry.QueryByTag(tag, records);

						Json::Value arr(Json::arrayValue);
						for (unsigned int i = 0; i < records.Size(); ++i)
							arr.append(RecordToJson(*records[i]));
						result["success"] = true;
						result["records"] = arr;
						return result;
					});

				// bulk_create_records — wraps N creates in a single CompoundCommand for atomic undo
				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.bulk_create_records"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("records") || !data["records"].isArray())
						{
							result["success"] = false;
							result["error"]   = "missing records array";
							return result;
						}

						const Json::Value& records = data["records"];
						if (records.size() == 0)
						{
							result["success"] = true;
							result["created"] = 0;
							return result;
						}

						mHistory.BeginCompound();
						int created = 0;
						for (unsigned int i = 0; i < records.size(); ++i)
						{
							Dia::AssetCatalogue::AssetRecord rec = RecordFromJson(records[i]);
							if (rec.mId == Dia::Core::StringCRC())
								continue;
							if (!rec.mSourcePath.IsEmpty())
								rec.mContentHash = mContentHasher.ComputeHash(rec.mSourcePath.AsCStr());
							auto* cmd = new Dia::AssetCatalogue::Editor::CreateRecordCommand(mRegistry, rec);
							mHistory.ExecuteCommand(cmd);
							++created;
						}
						mHistory.EndCompound();

						result["success"] = true;
						result["created"] = created;
						PushRegistryState();
						return result;
					});
			}
		}
	}
}

using namespace Dia::AssetCatalogue::Editor;

REGISTER_EDITOR_PLUGIN(DiaAssetCatalogueEditorPlugin, "DiaAssetCatalogueEditorPlugin")
