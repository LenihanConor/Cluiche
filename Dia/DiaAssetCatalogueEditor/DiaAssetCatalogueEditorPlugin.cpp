#include "DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h"
#include "DiaAssetCatalogueEditor/Commands/CreateRecordCommand.h"
#include "DiaAssetCatalogueEditor/Commands/UpdateRecordCommand.h"
#include "DiaAssetCatalogueEditor/Commands/DeleteRecordCommand.h"
#include "DiaAssetCatalogueEditor/Handlers/FileDiscoverer.h"
#include "DiaAssetCatalogueEditor/Commands/AddRelationshipCommand.h"
#include "DiaAssetCatalogueEditor/Commands/RemoveRelationshipCommand.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>

#include <cstring>
#include <string>

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

				mBridge = context.mBridge;
				mView   = context.mView;

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
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.discover_files"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.add_relationship"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.remove_relationship"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.get_forward_refs"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.get_reverse_refs"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.validate"));
				}

				mSessionContext.Save(mOutputDir);
				mBridge = nullptr;
				mView   = nullptr;
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
						std::string path = data["path"].asString();

						char errorBuf[256] = {};
						bool ok = mLoadHandler.Load(path.c_str(), mRegistry, mSerializer, mHistory,
							errorBuf, sizeof(errorBuf));

						if (ok)
						{
							strncpy_s(mCurrentPath, kCurrentPathLength, path.c_str(), _TRUNCATE);
							mSessionContext.SetLastManifestPath(path.c_str());
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
						std::string path = mCurrentPath;
						if (data.isMember("path") && data["path"].isString())
							path = data["path"].asString();

						if (path.empty())
						{
							result["success"] = false;
							result["error"]   = "no path specified";
							return result;
						}

						char errorBuf[256] = {};
						bool ok = mLoadHandler.Save(path.c_str(), mRegistry, mSerializer, mHistory,
							errorBuf, sizeof(errorBuf));

						if (ok)
						{
							strncpy_s(mCurrentPath, kCurrentPathLength, path.c_str(), _TRUNCATE);
							mSessionContext.SetLastManifestPath(path.c_str());
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
					std::string s = d["status"].asString();
					if (s == "Draft")       rec.mStatus = AssetStatus::Draft;
					else if (s == "Deprecated") rec.mStatus = AssetStatus::Deprecated;
					else                    rec.mStatus = AssetStatus::Active;
				}
				if (d.isMember("scope") && d["scope"].isString())
				{
					if (d["scope"].asString() == "stage")
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

						if (!mRegistry.FindById(fromId) || !mRegistry.FindById(toId))
						{
							result["success"] = false;
							result["error"]   = "record not found";
							return result;
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

						std::string rootPath = data["root_path"].asString();

						Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles> discovered;
						mFileDiscoverer.Discover(rootPath.c_str(), mTypeRegistry, mRegistry, discovered);

						Json::Value files(Json::arrayValue);
						for (unsigned int i = 0; i < discovered.Size(); ++i)
						{
							const DiscoveredFile& df = discovered[i];
							Json::Value entry;
							entry["path"]         = df.mFullPath;
							entry["suggested_type"] = df.mSuggestedType;
							entry["suggested_id"]   = df.mSuggestedId;
							files.append(entry);
						}

						result["success"] = true;
						result["files"]   = files;
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
									std::string msg = "References unknown asset: ";
									msg += targetId.AsChar();
									e["message"]  = msg.c_str();
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
			}
		}
	}
}

using namespace Dia::AssetCatalogue::Editor;

REGISTER_EDITOR_PLUGIN(DiaAssetCatalogueEditorPlugin, "DiaAssetCatalogueEditorPlugin")
