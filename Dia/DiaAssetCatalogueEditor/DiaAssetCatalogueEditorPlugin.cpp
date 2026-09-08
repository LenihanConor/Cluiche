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
#include <DiaEditor/UI/FileDialogHandler.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>
#include <DiaAssetCatalogue/BuiltInAssetTypes.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

#include <cstdio>
#include <cstring>
#include <sstream>
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
			DiaAssetCatalogueEditorPlugin::DiaAssetCatalogueEditorPlugin()
				: EditorPluginBase({
					"DiaAssetCatalogueEditor",
					"1.0.0",
					"Author and maintain the asset catalogue manifest",
					"dia://plugins/assetcatalogue/index.html",
					Dia::Editor::LayoutMode::kDockable,
					"assetcatalogue.state",
					nullptr,
					true
				})
			{
			}

			void DiaAssetCatalogueEditorPlugin::OnProjectChanged(const Dia::Editor::ProjectContext& ctx)
			{
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnProjectChanged — IsValid=%d diagamePath='%s' assetCataloguePath='%s'",
					ctx.IsValid() ? 1 : 0, ctx.diagamePath, ctx.assetCataloguePath);

				// Keep mDiagameDir in sync with the current project
				mDiagameDir[0] = '\0';
				if (ctx.IsValid() && ctx.diagamePath[0] != '\0')
				{
					const char* p = ctx.diagamePath;
					int lastSlash = -1;
					for (int i = 0; p[i] != '\0'; ++i)
						if (p[i] == '/' || p[i] == '\\') lastSlash = i;
					if (lastSlash >= 0)
						strncpy_s(mDiagameDir, kDiagameDirLength,
						          ctx.diagamePath, static_cast<size_t>(lastSlash + 1));
				}

				if (ctx.IsValid() && ctx.assetCataloguePath[0] != '\0')
				{
					char err[256] = {};
					DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: loading catalogue from '%s'", ctx.assetCataloguePath);
					if (!LoadManifestFromPath(ctx.assetCataloguePath, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: failed to load catalogue '%s': %s",
							ctx.assetCataloguePath, err);
						if (GetBridge())
							GetBridge()->NotifyUIDataChanged("assetcatalogue.status",
								Json::Value(err[0] ? err : "Failed to load catalogue"));
					}
					else
					{
						DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: catalogue loaded OK, %u records", mRegistry.GetCount());
					}
				}
				else
				{
					if (ctx.IsValid())
						DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: project '%s' has no asset_catalogue field",
							ctx.diagamePath);
					if (GetBridge())
						GetBridge()->NotifyUIDataChanged("assetcatalogue.status",
							Json::Value(ctx.IsValid() ? "No asset_catalogue configured in .diagame" : "No project loaded"));
				}
			}

			void DiaAssetCatalogueEditorPlugin::OnPluginLoad()
			{
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnPluginLoad");

				strncpy_s(mOutputDir, kOutputDirLength, kDefaultOutputDir, _TRUNCATE);
				mCurrentPath[0] = '\0';
				mDiagameDir[0]  = '\0';

				mSessionContext.Load(mOutputDir);

				Dia::AssetCatalogue::RegisterBuiltInAssetTypes(mTypeRegistry);

				if (GetModel() != nullptr)
				{
					const Dia::Editor::ProjectContext& proj = GetModel()->GetDiagameProject();

					// Populate mDiagameDir from current project (same logic as OnProjectChanged)
					if (proj.IsValid() && proj.diagamePath[0] != '\0')
					{
						const char* p = proj.diagamePath;
						int lastSlash = -1;
						for (int i = 0; p[i] != '\0'; ++i)
							if (p[i] == '/' || p[i] == '\\') lastSlash = i;
						if (lastSlash >= 0)
							strncpy_s(mDiagameDir, kDiagameDirLength,
							          proj.diagamePath, static_cast<size_t>(lastSlash + 1));
					}

					DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnPluginLoad — checking current project: IsValid=%d diagamePath='%s' assetCataloguePath='%s'",
						proj.IsValid() ? 1 : 0, proj.diagamePath, proj.assetCataloguePath);
					if (proj.IsValid() && proj.assetCataloguePath[0] != '\0')
					{
						char err[256] = {};
						DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnPluginLoad — loading catalogue from '%s'", proj.assetCataloguePath);
						if (!LoadManifestFromPath(proj.assetCataloguePath, err, sizeof(err)))
							DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: OnPluginLoad — failed to load catalogue: %s", err);
						else
							DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnPluginLoad — catalogue loaded OK, %u records", mRegistry.GetCount());
					}
					else
					{
						DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: OnPluginLoad — no catalogue to load (no project or no asset_catalogue field)");
					}
				}
				else
				{
					DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: OnPluginLoad — GetModel() is null");
				}

				SeedAssetTemplates();
				RegisterRequestHandlers();
				DualRegisterActions();

				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: Initialized");
			}

			void DiaAssetCatalogueEditorPlugin::OnPluginUnload()
			{
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnPluginUnload");
				if (GetServices() != nullptr)
				{
					Dia::Editor::EditorActionRegistryService* regSvc =
						GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
					if (regSvc != nullptr && regSvc->GetRegistry() != nullptr)
					{
						regSvc->GetRegistry()->DeregisterActionsForOwner(
							Dia::Core::StringCRC("DiaAssetCatalogueEditorPlugin"));
					}
				}
				mSessionContext.Save(mOutputDir);
			}

			void DiaAssetCatalogueEditorPlugin::OnUpdate(float /*deltaTime*/)
			{
			}

			void DiaAssetCatalogueEditorPlugin::OnNavigate(const Dia::Core::StringCRC& instanceId)
			{
				DIA_TRACE_ZONE("asset_catalogue.navigate_to_record", Dia::Observation::Trace::Category::kNone);
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin::OnNavigate: instanceId='%s'", instanceId.AsChar());

				if (!GetBridge())
					return;

				// kZero means "restore default state, no specific record to navigate to"
				if (instanceId.Value() == 0)
					return;

				// Verify the record exists before pushing
				const Dia::AssetCatalogue::AssetRecord* rec = mRegistry.FindById(instanceId);
				if (!rec)
				{
					DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin::OnNavigate: record '%s' not found in registry", instanceId.AsChar());
					return;
				}

				Json::Value payload;
				payload["id"] = instanceId.AsChar();
				GetBridge()->NotifyUIDataChanged("asset_catalogue.navigate_to_record", payload);
			}

			void DiaAssetCatalogueEditorPlugin::SeedAssetTemplates()
			{
				static const char* kBlankScene =
					"{\n"
					"    \"version\": 1,\n"
					"    \"entities\": [],\n"
					"    \"cameras\": [],\n"
					"    \"lights\": [],\n"
					"    \"layers\": []\n"
					"}\n";

				static const char* kBlankEntityTemplate =
					"{\n"
					"    \"entity_template\": {\n"
					"        \"id\": \"\",\n"
					"        \"components\": []\n"
					"    }\n"
					"}\n";

				static const char* kBlankCamera =
					"{\n"
					"    \"camera_blueprint\": {\n"
					"        \"id\": \"\",\n"
					"        \"components\": []\n"
					"    }\n"
					"}\n";

				static const char* kBlankLight =
					"{\n"
					"    \"light_blueprint\": {\n"
					"        \"id\": \"\",\n"
					"        \"components\": []\n"
					"    }\n"
					"}\n";

				mAssetTemplates[Dia::Core::StringCRC("diascene")]           = { kBlankScene,          ".diascene" };
				mAssetTemplates[Dia::Core::StringCRC("diaentitytemplate")]  = { kBlankEntityTemplate,  ".diaentitytemplate" };
				mAssetTemplates[Dia::Core::StringCRC("diacamera")]          = { kBlankCamera,          ".diacamera" };
				mAssetTemplates[Dia::Core::StringCRC("dialight")]           = { kBlankLight,           ".dialight" };
			}

			void DiaAssetCatalogueEditorPlugin::RegisterRequestHandlers()
			{
				if (!GetBridge())
					return;

				RegisterCRUDHandlers();
				RegisterDiscovererHandlers();
				RegisterRelationshipHandlers();
				RegisterValidationHandlers();
				RegisterAssetTypeEditorHandlers();
				RegisterRulesHandlers();
				RegisterInferrerHandlers();

				// Seed built-in type→editor mappings so open_asset works regardless of
				// plugin load order (DiaEntityTemplateEditor/DiaSceneEditor may load after us).
				mTypeEditorRegistry.RegisterTypeEditor(
					Dia::Core::StringCRC("diaentitytemplate"), Dia::Core::StringCRC("DiaEntityTemplateEditor"));
				mTypeEditorRegistry.RegisterTypeEditor(
					Dia::Core::StringCRC("diacamera"), Dia::Core::StringCRC("DiaEntityTemplateEditor"));
				mTypeEditorRegistry.RegisterTypeEditor(
					Dia::Core::StringCRC("dialight"),  Dia::Core::StringCRC("DiaEntityTemplateEditor"));
				mTypeEditorRegistry.RegisterTypeEditor(
					Dia::Core::StringCRC("diascene"),  Dia::Core::StringCRC("DiaSceneEditor"));
				mTypeEditorRegistry.RegisterTypeEditor(
					Dia::Core::StringCRC("stage"),     Dia::Core::StringCRC("DiaApplicationFlowEditor"));

				RegisterHandler(
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

						char errorBuf[256] = {};
						if (LoadManifestFromPath(data["path"].asCString(), errorBuf, sizeof(errorBuf)))
						{
							result["success"]      = true;
							result["record_count"] = static_cast<int>(mRegistry.GetCount());
						}
						else
						{
							result["success"] = false;
							result["error"]   = errorBuf[0] ? errorBuf : "load failed";
						}
						return result;
					});

				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.browse_open"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value dialogData;
						Json::Value filters(Json::arrayValue);
						Json::Value f1; f1["name"] = "Asset Catalogue"; f1["ext"] = "*.json"; filters.append(f1);
						Json::Value f2; f2["name"] = "Dia Game Project"; f2["ext"] = "*.diagame"; filters.append(f2);
						Json::Value f3; f3["name"] = "All Files"; f3["ext"] = "*.*"; filters.append(f3);
						dialogData["filters"] = filters;
						dialogData["default_ext"] = "json";
						dialogData["title"] = "Open Asset Catalogue";

						Json::Value dialogResult = Dia::Editor::FileDialogHandler::HandleOpenFileDialog(dialogData);
						if (!dialogResult.get("success", false).asBool())
						{
							Json::Value r;
							r["success"] = false;
							return r;
						}

						char errorBuf[256] = {};
						Json::Value result;
						if (LoadManifestFromPath(dialogResult["path"].asCString(), errorBuf, sizeof(errorBuf)))
						{
							result["success"] = true;
						}
						else
						{
							result["success"] = false;
							result["error"] = errorBuf[0] ? errorBuf : "load failed";
						}
						return result;
					});

				// browse_source_folder — folder picker pre-seeded to the manifest directory.
				// Returns { success, path, relative_path } where relative_path is the
				// chosen folder relative to the manifest (use as a directory prefix for source_path).
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.browse_source_file"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value dialogData;
						dialogData["title"] = "Select Folder for Source Path";

						// Prefer the .diagame directory; fall back to manifest directory
						if (mDiagameDir[0] != '\0')
							dialogData["initial_dir"] = mDiagameDir;
						else
						{
							char dirBuf[512] = {};
							GetManifestDirectory(dirBuf, sizeof(dirBuf));
							if (dirBuf[0] != '\0')
								dialogData["initial_dir"] = dirBuf;
						}

						if (data.isMember("initial_dir") && data["initial_dir"].isString())
							dialogData["initial_dir"] = data["initial_dir"].asString();

						Json::Value dialogResult = Dia::Editor::FileDialogHandler::HandleFolderDialog(dialogData);
						if (!dialogResult.get("success", false).asBool())
						{
							Json::Value r; r["success"] = false; return r;
						}

						const char* absPath = dialogResult["path"].asCString();
						char relBuf[512] = {};
						MakeRelativeToManifest(absPath, relBuf, sizeof(relBuf));

						// Ensure trailing slash so the UI can append a filename directly
						unsigned int relLen = static_cast<unsigned int>(strlen(relBuf));
						if (relLen > 0 && relBuf[relLen - 1] != '/' && relBuf[relLen - 1] != '\\')
						{
							if (relLen + 1 < 512) { relBuf[relLen] = '/'; relBuf[relLen + 1] = '\0'; }
						}

						Json::Value result;
						result["success"]       = true;
						result["path"]          = absPath;
						result["relative_path"] = relBuf;
						return result;
					});

				// get_manifest_dir — returns the directory of the currently loaded manifest.
				// Used by the UI to build default source paths for new records.
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.get_manifest_dir"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value result;
						char dirBuf[512] = {};
						GetManifestDirectory(dirBuf, sizeof(dirBuf));
						result["success"] = true;
						result["dir"]     = dirBuf;
						return result;
					});

				RegisterHandler(
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

				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.new_manifest"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						mLoadHandler.NewManifest(mRegistry, mHistory);
						mCurrentPath[0] = '\0';
						PushRegistryState();

						Json::Value result;
						result["success"] = true;
						return result;
					});

				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.get_state"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value result;
						result["success"] = true;
						result["path"]    = mCurrentPath;
						result["dirty"]   = mLoadHandler.IsDirty(mHistory);

						Json::Value records(Json::arrayValue);
						for (unsigned int i = 0; i < mRegistry.GetCount(); ++i)
							records.append(RecordToJsonWithMeta(mRegistry.GetRecordByIndex(i)));
						result["records"] = records;

						if (mCurrentPath[0] == '\0' && mRegistry.GetCount() == 0)
							result["status"] = "No catalogue loaded — open a project with asset_catalogue configured in .diagame";

						return result;
					});
			}

			void DiaAssetCatalogueEditorPlugin::PushDirtyState()
			{
				if (!GetBridge())
					return;
				Json::Value data;
				data["dirty"] = mLoadHandler.IsDirty(mHistory);
				data["path"]  = mCurrentPath;
				GetBridge()->NotifyUIDataChanged("assetcatalogue.state", data);
			}

			void DiaAssetCatalogueEditorPlugin::PushRegistryState()
			{
				if (!GetBridge())
					return;
				Json::Value records(Json::arrayValue);
				for (unsigned int i = 0; i < mRegistry.GetCount(); ++i)
					records.append(RecordToJsonWithMeta(mRegistry.GetRecordByIndex(i)));
				GetBridge()->NotifyUIDataChanged("assetcatalogue.records", records);
				GetBridge()->NotifyUIDataChanged("asset_catalogue.registry_changed", Json::Value(Json::objectValue));
				PushDirtyState();
			}

			bool DiaAssetCatalogueEditorPlugin::LoadManifestFromPath(const char* path, char* errorOut, unsigned int errorCapacity)
			{
				bool ok = mLoadHandler.Load(path, mRegistry, mSerializer, mHistory,
					errorOut, errorCapacity);
				if (!ok)
					return false;

				strncpy_s(mCurrentPath, kCurrentPathLength, path, _TRUNCATE);
				mSessionContext.SetLastManifestPath(path);
				mSessionContext.Save(mOutputDir);
				PushRegistryState();
				AutoLoadRules();
				return true;
			}

			void DiaAssetCatalogueEditorPlugin::GetManifestDirectory(char* dirOut, unsigned int dirOutSize) const
			{
				dirOut[0] = '\0';
				int lastSlash = -1;
				for (int i = 0; mCurrentPath[i] != '\0'; ++i)
				{
					if (mCurrentPath[i] == '/' || mCurrentPath[i] == '\\')
						lastSlash = i;
				}
				if (lastSlash >= 0)
					strncpy_s(dirOut, dirOutSize, mCurrentPath, static_cast<size_t>(lastSlash + 1));
			}

			void DiaAssetCatalogueEditorPlugin::MakeRelativeToManifest(const char* absPath, char* relOut, unsigned int relOutSize) const
			{
				char dirBuf[512] = {};
				GetManifestDirectory(dirBuf, sizeof(dirBuf));
				unsigned int dirLen = static_cast<unsigned int>(strlen(dirBuf));

				// If absPath starts with the manifest directory, strip it
				if (dirLen > 0 && _strnicmp(absPath, dirBuf, dirLen) == 0)
				{
					strncpy_s(relOut, relOutSize, absPath + dirLen, _TRUNCATE);
				}
				else
				{
					strncpy_s(relOut, relOutSize, absPath, _TRUNCATE);
				}
			}

			void DiaAssetCatalogueEditorPlugin::AutoLoadRules()
			{
				if (!mSerializer.HasRulesPath())
					return;

				char dirBuf[512] = {};
				GetManifestDirectory(dirBuf, sizeof(dirBuf));

				char resolvedPath[512] = {};
				_snprintf_s(resolvedPath, sizeof(resolvedPath), _TRUNCATE, "%s%s", dirBuf, mSerializer.GetRulesPath());

				Dia::AssetCatalogue::LoadResult<void> lr = mRulesEngine.LoadRules(resolvedPath, mTypeRegistry);
				if (lr.mSuccess)
				{
					DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: auto-loaded %d rules from %s",
						mRulesEngine.GetRuleCount(), resolvedPath);

					if (GetBridge())
					{
						Json::Value data;
						data["rules_path"] = resolvedPath;
						data["rule_count"] = static_cast<int>(mRulesEngine.GetRuleCount());
						GetBridge()->NotifyUIDataChanged("assetcatalogue.rulesLoaded", data);
					}
				}
				else
				{
					DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: failed to auto-load rules from %s",
						resolvedPath);

					if (GetBridge())
					{
						Json::Value data;
						data["rules_path"] = resolvedPath;
						data["error"] = lr.HasErrors() ? lr.GetFirstError().mMessage.AsCStr() : "load failed";
						GetBridge()->NotifyUIDataChanged("assetcatalogue.rulesLoadFailed", data);
					}
				}
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

			Json::Value DiaAssetCatalogueEditorPlugin::RecordToJsonWithMeta(const Dia::AssetCatalogue::AssetRecord& rec) const
			{
				Json::Value d = RecordToJson(rec);
				d["has_editor"] = (mTypeEditorRegistry.FindEditorForType(rec.mAssetTypeId) != Dia::Core::StringCRC());
				return d;
			}

			// relationship handlers
			void DiaAssetCatalogueEditorPlugin::RegisterRelationshipHandlers()
			{
				if (!GetBridge())
					return;

				// add_relationship
				RegisterHandler(
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

						// Auto-save
						if (mCurrentPath[0] != '\0')
						{
							char saveErr[256] = {};
							if (mLoadHandler.Save(mCurrentPath, mRegistry, mSerializer, mHistory,
							    saveErr, sizeof(saveErr)))
								PushDirtyState();
							else
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: add_relationship — auto-save failed: %s", saveErr);
						}

						return result;
					});

				// remove_relationship
				RegisterHandler(
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

						// Auto-save
						if (mCurrentPath[0] != '\0')
						{
							char saveErr[256] = {};
							if (mLoadHandler.Save(mCurrentPath, mRegistry, mSerializer, mHistory,
							    saveErr, sizeof(saveErr)))
								PushDirtyState();
							else
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: remove_relationship — auto-save failed: %s", saveErr);
						}

						return result;
					});

				// get_forward_refs (read-only query)
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.get_forward_refs"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("id") || !data["id"].isString() || data["id"].asString().empty())
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
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.get_reverse_refs"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("id") || !data["id"].isString() || data["id"].asString().empty())
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
				if (!GetBridge())
					return;

				RegisterHandler(
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
				if (!GetBridge())
					return;

				// browse_rules — open file dialog then load
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.browse_rules"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value dialogData;
						Json::Value filters(Json::arrayValue);
						Json::Value f1; f1["name"] = "Rules Files"; f1["ext"] = "*.rules.json"; filters.append(f1);
						Json::Value f2; f2["name"] = "JSON Files"; f2["ext"] = "*.json"; filters.append(f2);
						Json::Value f3; f3["name"] = "All Files"; f3["ext"] = "*.*"; filters.append(f3);
						dialogData["filters"] = filters;
						dialogData["default_ext"] = "json";
						dialogData["title"] = "Open Rules File";

						Json::Value dialogResult = Dia::Editor::FileDialogHandler::HandleOpenFileDialog(dialogData);
						if (!dialogResult.get("success", false).asBool())
						{
							Json::Value r;
							r["success"] = false;
							return r;
						}

						const char* rulesPath = dialogResult["path"].asCString();
						Dia::AssetCatalogue::LoadResult<void> lr = mRulesEngine.LoadRules(rulesPath, mTypeRegistry);

						Json::Value result;
						if (!lr.mSuccess)
						{
							result["success"] = false;
							result["error"]   = lr.HasErrors() ? lr.GetFirstError().mMessage.AsCStr() : "load failed";
						}
						else
						{
							// Store as relative path so it persists correctly on save
							char relPath[256] = {};
							MakeRelativeToManifest(rulesPath, relPath, sizeof(relPath));
							mSerializer.SetRulesPath(relPath);

							result["success"]    = true;
							result["path"]       = rulesPath;
							result["rule_count"] = static_cast<int>(mRulesEngine.GetRuleCount());
						}
						return result;
					});

				// load_rules
				RegisterHandler(
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
				RegisterHandler(
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
				RegisterHandler(
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

						// Auto-save
						if (mCurrentPath[0] != '\0')
						{
							char saveErr[256] = {};
							if (mLoadHandler.Save(mCurrentPath, mRegistry, mSerializer, mHistory,
							    saveErr, sizeof(saveErr)))
								PushDirtyState();
							else
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: apply_rules — auto-save failed: %s", saveErr);
						}

						return result;
					});

				// get_rules — returns loaded rules list for UI display
				RegisterHandler(
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
				RegisterHandler(
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

						const unsigned int maxResults = data.isMember("limit") && data["limit"].isUInt()
							? data["limit"].asUInt() : 10u;
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
							if (ids.size() >= maxResults)
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
				if (!GetBridge())
					return;

				// get_asset_types — returns all registered type descriptors for the New dialog dropdown.
				// AssetTypeRegistry has no GetByIndex; we probe the known built-in IDs in order.
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.get_asset_types"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						static const struct { const char* id; const char* name; } kKnown[] = {
							{ "texture",   "Texture" },
							{ "sprite",    "Mesh/Sprite" },
							{ "audio",     "Audio" },
							{ "config",    "Config" },
							{ "diaentitytemplate", "Entity" },
							{ "stage",     "Stage" },
							{ "ui",        "UI Definition" },
							{ "folder",    "Folder" },
							{ "diacamera", "Camera Blueprint" },
							{ "dialight",  "Light Blueprint" },
							{ "diascene",  "Scene" },
						};
						static const unsigned int kCount = 11;

						Json::Value result;
						result["success"] = true;
						Json::Value types(Json::arrayValue);
						for (unsigned int i = 0; i < kCount; ++i)
						{
							if (mTypeRegistry.FindByTypeId(Dia::Core::StringCRC(kKnown[i].id)) != nullptr)
							{
								Json::Value entry;
								entry["typeId"] = kKnown[i].id;
								entry["name"]   = kKnown[i].name;
								types.append(entry);
							}
						}
						result["types"] = types;
						return result;
					});

				// register_type_editor — allows other plugins to bind a type to an editor
				RegisterHandler(
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

				// create_scene — redirects to create_asset; also auto-opens in DiaSceneEditor
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.create_scene"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value fwd = data;
						fwd["assetType"] = "diascene";
						Json::Value r = GetBridge()->InvokeRequestHandler(
							Dia::Core::StringCRC("asset_catalogue.create_asset"), fwd);
						if (!r.isNull() && r.get("success", false).asBool())
						{
							if (GetPluginLoader())
								GetPluginLoader()->LoadPlugin(
									Dia::Core::StringCRC("DiaSceneEditor"),
									Dia::Core::StringCRC(data["id"].asCString()));
						}
						return r.isNull() ? MakeErrorResponse("create_asset not available") : r;
					});

				// create_asset — write a blank asset file from a registered template, register record
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.create_asset"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("assetType") || !data["assetType"].isString()
							|| !data.isMember("id") || !data["id"].isString()
							|| !data.isMember("source_path") || !data["source_path"].isString())
						{
							result["success"] = false;
							result["error"]   = "missing assetType, id, or source_path";
							return result;
						}

						const char* assetType = data["assetType"].asCString();
						Dia::Core::StringCRC assetTypeCRC(assetType);
						auto it = mAssetTemplates.find(assetTypeCRC);
						if (it == mAssetTemplates.end())
						{
							result["success"] = false;
							result["error"]   = "unknown assetType";
							return result;
						}

						const char* relPath = data["source_path"].asCString();

						// Resolve absolute path from diagame directory
						char absPath[1024] = {};
						bool pathIsAbsolute = (relPath[0] == '/' || relPath[0] == '\\'
							|| (relPath[0] != '\0' && relPath[1] == ':'));
						if (!pathIsAbsolute && mDiagameDir[0] != '\0')
							snprintf(absPath, sizeof(absPath), "%s%s", mDiagameDir, relPath);
						else
							strncpy_s(absPath, sizeof(absPath), relPath, _TRUNCATE);

						// Write template content to file
						FILE* f = nullptr;
						if (fopen_s(&f, absPath, "wb") != 0 || !f)
						{
							DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: create_asset — could not write '%s'", absPath);
							result["success"] = false;
							result["error"]   = "could not write asset file";
							return result;
						}
						fputs(it->second.content, f);
						fclose(f);

						// Register catalogue record via CreateRecordCommand
						Dia::AssetCatalogue::AssetRecord rec;
						rec.mId          = Dia::Core::StringCRC(data["id"].asCString());
						rec.mAssetTypeId = assetTypeCRC;
						rec.mSourcePath  = relPath;
						rec.mStatus      = Dia::AssetCatalogue::AssetStatus::Active;
						rec.mScope       = Dia::AssetCatalogue::AssetScope::kGlobal;

						if (data.isMember("tags") && data["tags"].isArray())
						{
							for (const Json::Value& tag : data["tags"])
							{
								if (tag.isString() && rec.mTags.Size() < rec.mTags.Capacity())
									rec.mTags.Add(Dia::Core::StringCRC(tag.asCString()));
							}
							if (rec.mTags.Size() > 0)
								rec.mManualOverrideFlags |= Dia::AssetCatalogue::kManualOverrideTags;
						}

						auto* cmd = new Dia::AssetCatalogue::Editor::CreateRecordCommand(mRegistry, rec);
						mHistory.ExecuteCommand(cmd);
						PushRegistryState();

						DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: created asset '%s' (type '%s') at '%s'",
							data["id"].asCString(), assetType, absPath);

						// Auto-save manifest
						if (mCurrentPath[0] != '\0')
						{
							char saveErr[256] = {};
							if (mLoadHandler.Save(mCurrentPath, mRegistry, mSerializer, mHistory,
								saveErr, sizeof(saveErr)))
								PushDirtyState();
							else
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: create_asset — auto-save failed: %s", saveErr);
						}

						result["success"] = true;
						result["id"]      = data["id"];
						result["absPath"] = absPath;
						return result;
					});

				// get_record — look up a single record by id; used by other plugins for deep-link navigation
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.get_record"),
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
						result["success"]     = true;
						result["record"]      = RecordToJsonWithMeta(*rec);
						return result;
					});

				// open_in_editor — dispatch to registered editor plugin only
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.open_in_editor"),
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
						Dia::Core::StringCRC editorPluginType = mTypeEditorRegistry.FindEditorForType(rec->mAssetTypeId);
						if (editorPluginType == Dia::Core::StringCRC() || !GetPluginLoader())
						{
							result["success"] = false;
							result["error"]   = "no editor registered for this asset type";
							return result;
						}
						GetPluginLoader()->LoadPlugin(editorPluginType, assetId);
						result["success"] = true;
						return result;
					});

				// open_in_file — open source file with OS default handler
				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.open_in_file"),
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
						if (rec->mSourcePath.IsEmpty())
						{
							result["success"] = false;
							result["error"]   = "no source path for this record";
							return result;
						}
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
						return result;
					});
			}

			// validate handler
			void DiaAssetCatalogueEditorPlugin::RegisterValidationHandlers()
			{
				if (!GetBridge())
					return;

				RegisterHandler(
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
				if (!GetBridge())
					return;

				RegisterHandler(
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

						// For types with blank-file templates, write the file if it doesn't exist yet.
						if (!rec.mSourcePath.IsEmpty())
						{
							const char* typeStr = rec.mAssetTypeId.AsChar();
							bool isFileType = (strcmp(typeStr, "diaentitytemplate") == 0
							                || strcmp(typeStr, "diacamera") == 0
							                || strcmp(typeStr, "dialight")  == 0
							                || strcmp(typeStr, "diascene")  == 0);
							if (isFileType)
							{
								char absPath[1024] = {};
								const char* srcPath = rec.mSourcePath.AsCStr();
								bool isAbsolute = (srcPath[0] == '/' || srcPath[0] == '\\' ||
								                   (srcPath[0] != '\0' && srcPath[1] == ':'));
								if (!isAbsolute && mDiagameDir[0] != '\0')
									snprintf(absPath, sizeof(absPath), "%s%s", mDiagameDir, srcPath);
								else
									strncpy_s(absPath, sizeof(absPath), srcPath, _TRUNCATE);

								DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: create_record — resolved path: diagameDir='%s' sourcePath='%s' absPath='%s'",
									mDiagameDir, rec.mSourcePath.AsCStr(), absPath);

								FILE* probe = nullptr;
								if (fopen_s(&probe, absPath, "rb") != 0 || !probe)
								{
									// File doesn't exist — write blank template
									char content[512];
									if (strcmp(typeStr, "diascene") == 0)
									{
										snprintf(content, sizeof(content),
											"{\n    \"version\": 1,\n    \"entities\": [],\n    \"cameras\": [],\n    \"lights\": [],\n    \"layers\": []\n}\n");
									}
									else
									{
										const char* topKey = "entity_template";
										if (strcmp(typeStr, "diacamera") == 0) topKey = "camera_blueprint";
										else if (strcmp(typeStr, "dialight") == 0) topKey = "light_blueprint";

										snprintf(content, sizeof(content),
											"{\n    \"%s\": {\n        \"id\": \"%s\",\n        \"components\": []\n    }\n}\n",
											topKey, rec.mId.AsChar());
									}

									FILE* f = nullptr;
									if (fopen_s(&f, absPath, "wb") == 0 && f)
									{
										fputs(content, f);
										fclose(f);
										DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: created blank %s at '%s'", typeStr, absPath);
									}
									else
									{
										DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: could not write blank %s to '%s'", typeStr, absPath);
									}
								}
								else
								{
									fclose(probe);
								}
							}
						}

						// Compute content hash
						if (!rec.mSourcePath.IsEmpty())
							rec.mContentHash = mContentHasher.ComputeHash(rec.mSourcePath.AsCStr());

						auto* cmd = new Dia::AssetCatalogue::Editor::CreateRecordCommand(mRegistry, rec);
						mHistory.ExecuteCommand(cmd);

						result["success"]      = true;
						result["content_hash"] = static_cast<Json::UInt>(rec.mContentHash);
						PushRegistryState();

						// Auto-save manifest after record creation
						if (mCurrentPath[0] != '\0')
						{
							char saveErr[256] = {};
							if (mLoadHandler.Save(mCurrentPath, mRegistry, mSerializer, mHistory,
							    saveErr, sizeof(saveErr)))
								PushDirtyState();
							else
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: create_record — auto-save failed: %s", saveErr);
						}

						return result;
					});

				RegisterHandler(
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

						// Auto-save manifest after record update
						if (mCurrentPath[0] != '\0')
						{
							char saveErr[256] = {};
							if (mLoadHandler.Save(mCurrentPath, mRegistry, mSerializer, mHistory,
							    saveErr, sizeof(saveErr)))
								PushDirtyState();
							else
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: update_record — auto-save failed: %s", saveErr);
						}

						return result;
					});

				RegisterHandler(
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

						// Auto-save manifest after record deletion
						if (mCurrentPath[0] != '\0')
						{
							char saveErr[256] = {};
							if (mLoadHandler.Save(mCurrentPath, mRegistry, mSerializer, mHistory,
							    saveErr, sizeof(saveErr)))
								PushDirtyState();
							else
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: delete_record — auto-save failed: %s", saveErr);
						}

						return result;
					});

				RegisterHandler(
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
							arr.append(RecordToJsonWithMeta(*records[i]));
						result["success"] = true;
						result["records"] = arr;
						return result;
					});

				RegisterHandler(
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
							arr.append(RecordToJsonWithMeta(*records[i]));
						result["success"] = true;
						result["records"] = arr;
						return result;
					});

				// bulk_create_records — wraps N creates in a single CompoundCommand for atomic undo
				RegisterHandler(
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

						// Auto-save
						if (mCurrentPath[0] != '\0')
						{
							char saveErr[256] = {};
							if (mLoadHandler.Save(mCurrentPath, mRegistry, mSerializer, mHistory,
							    saveErr, sizeof(saveErr)))
								PushDirtyState();
							else
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: bulk_create_records — auto-save failed: %s", saveErr);
						}

						return result;
					});
			}
			// inferrer handlers
			void DiaAssetCatalogueEditorPlugin::RegisterInferrerHandlers()
			{
				if (!GetBridge())
					return;

				RegisterHandler(
					Dia::Core::StringCRC("asset_catalogue.infer_relationships"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value result;
						int scenesScanned = 0;
						int edgesAdded    = 0;
						int edgesSkipped  = 0;

						// Resolve base directory for relative source paths
						char manifestDir[512] = {};
						GetManifestDirectory(manifestDir, sizeof(manifestDir));
						const char* baseDir = (mDiagameDir[0] != '\0') ? mDiagameDir : manifestDir;

						for (unsigned int i = 0; i < mRegistry.GetCount(); ++i)
						{
							const Dia::AssetCatalogue::AssetRecord& rec = mRegistry.GetRecordByIndex(i);
							if (rec.mAssetTypeId != Dia::Core::StringCRC("diascene"))
								continue;
							if (rec.mSourcePath.IsEmpty())
								continue;

							// Resolve absolute path
							char absPath[1024] = {};
							const char* srcPath = rec.mSourcePath.AsCStr();
							bool isAbsolute = (srcPath[0] == '/' || srcPath[0] == '\\' ||
							                   (srcPath[0] != '\0' && srcPath[1] == ':'));
							if (!isAbsolute && baseDir[0] != '\0')
								snprintf(absPath, sizeof(absPath), "%s%s", baseDir, srcPath);
							else
								strncpy_s(absPath, sizeof(absPath), srcPath, _TRUNCATE);

							// Parse the .diascene JSON file
							FILE* f = nullptr;
							if (fopen_s(&f, absPath, "rb") != 0 || !f)
							{
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: infer_relationships — cannot open '%s'", absPath);
								continue;
							}
							fseek(f, 0, SEEK_END);
							long fsize = ftell(f);
							fseek(f, 0, SEEK_SET);
							std::string content(static_cast<size_t>(fsize), '\0');
							fread(&content[0], 1, static_cast<size_t>(fsize), f);
							fclose(f);

							Json::Value sceneRoot;
							Json::CharReaderBuilder b;
							std::string parseErr;
							std::istringstream ss(content);
							if (!Json::parseFromStream(b, ss, &sceneRoot, &parseErr))
							{
								DIA_LOG_WARNING("Editor", "DiaAssetCatalogueEditorPlugin: infer_relationships — parse error in '%s': %s", absPath, parseErr.c_str());
								continue;
							}
							++scenesScanned;

							const Dia::Core::StringCRC sceneId = rec.mId;

							// Helper lambda to process an array of items
							auto processArray = [&](const char* key)
							{
								if (!sceneRoot.isMember("scene2d")) return;
								const Json::Value& arr = sceneRoot["scene2d"][key];
								if (!arr.isArray()) return;
								for (unsigned int j = 0; j < arr.size(); ++j)
								{
									if (!arr[j].isMember("blueprint")) continue;
									const Json::Value& bp = arr[j]["blueprint"];
									const char* bpId = bp.isString() ? bp.asCString()
									                 : (bp.isObject() && bp.isMember("value") ? bp["value"].asCString() : "");
									if (!bpId || bpId[0] == '\0') continue;

									// Only add if the blueprint record exists in the catalogue
									if (!mRegistry.FindById(Dia::Core::StringCRC(bpId)))
									{
										++edgesSkipped;
										continue;
									}

									Json::Value addReq;
									addReq["from"] = sceneId.AsChar();
									addReq["rel"]  = "uses";
									addReq["to"]   = bpId;
									Json::Value addResult = GetBridge()->InvokeRequestHandler(
										Dia::Core::StringCRC("asset_catalogue.add_relationship"), addReq);

									if (addResult.get("success", false).asBool())
										++edgesAdded;
									else
										++edgesSkipped;
								}
							};

							processArray("entities");
							processArray("cameras");
							processArray("lights");
						}

						result["success"]        = true;
						result["scenes_scanned"] = scenesScanned;
						result["edges_added"]    = edgesAdded;
						result["edges_skipped"]  = edgesSkipped;
						return result;
					});
			}

			void DiaAssetCatalogueEditorPlugin::DualRegisterActions()
			{
				if (GetServices() == nullptr) return;
				Dia::Editor::EditorActionRegistryService* regSvc =
					GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
				if (regSvc == nullptr || regSvc->GetRegistry() == nullptr) return;
				Dia::Editor::EditorActionRegistry* api = regSvc->GetRegistry();

				// Helper: register one action, delegate to existing WebUIBridge handler
				auto reg = [&](const char* name, const char* description,
				               Dia::Editor::DispatchThread dispatch,
				               Dia::Editor::EditorActionParam* paramsArr = nullptr,
				               unsigned int paramCount = 0)
				{
					Dia::Core::StringCRC handlerKey(name);
					Dia::Editor::EditorActionDescriptor d;
					d.name           = handlerKey;
					d.description    = description;
					d.category       = "asset_catalogue";
					d.owner          = "DiaAssetCatalogueEditorPlugin";
					d.dispatchThread = dispatch;
					d.handler        = [this, handlerKey](const Json::Value& data) -> Json::Value {
						if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
						return GetBridge()->InvokeRequestHandler(handlerKey, data);
					};
					for (unsigned int i = 0; i < paramCount; ++i)
						d.params.params.Add(paramsArr[i]);
					api->RegisterAction(d);
				};

				using DT = Dia::Editor::DispatchThread;

				// ── kCallerThread (read-only) ─────────────────────────────────────────────

				reg("asset_catalogue.get_state",
				    "Returns the full current state of the asset catalogue: the manifest path, dirty flag, "
				    "and the complete list of records. Use this to snapshot the catalogue before making "
				    "bulk changes, or to verify the effect of a previous mutation. Returns an empty records "
				    "array if no manifest is loaded.",
				    DT::kCallerThread);

				reg("asset_catalogue.get_manifest_dir",
				    "Returns the directory that contains the currently loaded manifest file. Use this as "
				    "the base when constructing relative asset source paths.",
				    DT::kCallerThread);

				{
					Dia::Editor::EditorActionParam p;
					p.name="initial_dir"; p.type="string"; p.required=false;
					p.description="Directory to open the picker in. Defaults to manifest dir.";
					reg("asset_catalogue.browse_source_file",
					    "Opens a file-picker dialog and returns the selected file path both as an absolute "
					    "path and as a path relative to the manifest directory. Use when the user needs to "
					    "point a record at a source file on disk.",
					    DT::kCallerThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="id"; p.type="string"; p.required=true;
					p.description="The unique asset ID.";
					reg("asset_catalogue.get_record",
					    "Returns the full record descriptor for a single asset by ID. Includes type, source "
					    "path, status, scope, stage, tags, content_hash, and relationship metadata.",
					    DT::kCallerThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="id"; p.type="string"; p.required=true;
					p.description="Source asset ID.";
					reg("asset_catalogue.get_forward_refs",
					    "Returns all outbound relationship edges from the given asset — every (rel, target) "
					    "pair where this asset is the source. Use to find what an asset depends on.",
					    DT::kCallerThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="id"; p.type="string"; p.required=true;
					p.description="Target asset ID.";
					reg("asset_catalogue.get_reverse_refs",
					    "Returns all inbound relationship edges to the given asset — every (rel, source) pair "
					    "where this asset is the target. Use to find what depends on an asset before deleting it.",
					    DT::kCallerThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="root_path"; p.type="string"; p.required=true;
					p.description="Absolute directory path to scan.";
					reg("asset_catalogue.discover_files",
					    "Walks the directory tree from root_path and returns every file with a suggested "
					    "asset type and ID derived from its path. Use as the first step of a bulk-import "
					    "workflow before calling bulk_create_records.",
					    DT::kCallerThread, &p, 1);
				}

				reg("asset_catalogue.dry_run_rules",
				    "Evaluates all loaded rules against the current catalogue and returns the proposed "
				    "changes without applying them. Use to preview the effect of apply_rules before "
				    "committing. The truncated flag is true if the result set was capped.",
				    DT::kCallerThread);

				reg("asset_catalogue.get_rules",
				    "Returns the list of currently loaded auto-categorisation rules: name, match criteria, "
				    "and action for each rule.",
				    DT::kCallerThread);

				{
					Dia::Editor::EditorActionParam params[3];
					params[0].name="prefix"; params[0].type="string"; params[0].required=false; params[0].description="ID prefix to match against.";
					params[1].name="typeId"; params[1].type="string"; params[1].required=false; params[1].description="Restrict to this asset type.";
					params[2].name="limit";  params[2].type="uint";   params[2].required=false; params[2].description="Maximum results to return (default: all).";
					reg("asset_catalogue.query_asset_ids",
					    "Returns a filtered list of asset IDs. Supports optional prefix match, type filter, "
					    "and result limit. Use for autocomplete or picking an asset ID without fetching full records.",
					    DT::kCallerThread, params, 3);
				}

				reg("asset_catalogue.get_asset_types",
				    "Returns all registered asset type definitions: their typeId and display name. Use "
				    "to populate type pickers or to validate a typeId before calling create_record.",
				    DT::kCallerThread);

				reg("asset_catalogue.validate",
				    "Validates the current catalogue: checks for missing source files, broken "
				    "relationships, duplicate IDs, and unknown types. Returns a list of errors and "
				    "warnings. An empty errors array means the catalogue is clean.",
				    DT::kCallerThread);

				{
					Dia::Editor::EditorActionParam p;
					p.name="typeId"; p.type="string"; p.required=true;
					p.description="Asset type ID to filter by, e.g. 'entity_template'.";
					reg("asset_catalogue.query_by_type",
					    "Returns all records whose type matches the given typeId. Use to enumerate all scenes, "
					    "all entity templates, etc.",
					    DT::kCallerThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="tag"; p.type="string"; p.required=true;
					p.description="Tag string to filter by.";
					reg("asset_catalogue.query_by_tag",
					    "Returns all records that carry the given tag string.",
					    DT::kCallerThread, &p, 1);
				}

				// get_available — NEW action (not a WebUIBridge handler)
				{
					Dia::Editor::EditorActionDescriptor d;
					d.name           = Dia::Core::StringCRC("asset_catalogue.get_available");
					d.description    = "Returns whether the Asset Catalogue plugin is currently loaded and ready. Call this "
					                   "before any asset_catalogue.* action to confirm the plugin is active. If not loaded, "
					                   "use plugin_browser.load to activate it first.";
					d.category       = "asset_catalogue";
					d.owner          = "DiaAssetCatalogueEditorPlugin";
					d.dispatchThread = DT::kCallerThread;
					d.handler        = [](const Json::Value& /*data*/) -> Json::Value {
						Json::Value r;
						r["loaded"] = true;  // if this handler runs, the plugin is loaded by definition
						return r;
					};
					api->RegisterAction(d);
				}

				// ── kMainThread (mutating) ────────────────────────────────────────────────

				{
					Dia::Editor::EditorActionParam p;
					p.name="path"; p.type="string"; p.required=true;
					p.description="Absolute path to the .diacatalogue manifest file.";
					reg("asset_catalogue.load_manifest",
					    "Loads an asset catalogue manifest from the given file path. Replaces the current "
					    "manifest in memory; any unsaved changes are discarded. The path must be an absolute "
					    "path to a valid .diacatalogue file.",
					    DT::kMainThread, &p, 1);
				}

				reg("asset_catalogue.browse_open",
				    "Opens a file-picker dialog for the user to select an asset catalogue manifest. "
				    "Equivalent to File > Open in the catalogue UI. Requires an active window; do not "
				    "call from headless scripts.",
				    DT::kMainThread);

				{
					Dia::Editor::EditorActionParam p;
					p.name="path"; p.type="string"; p.required=false;
					p.description="Absolute save path. Omit to save to the current path.";
					reg("asset_catalogue.save_manifest",
					    "Saves the current in-memory catalogue to disk. If path is provided it saves to that "
					    "location (Save As); otherwise saves to the currently loaded path. Returns an error "
					    "if no path is known and none is provided.",
					    DT::kMainThread, &p, 1);
				}

				reg("asset_catalogue.new_manifest",
				    "Clears the in-memory catalogue and creates a fresh empty manifest. Any unsaved "
				    "changes to the previous manifest are discarded. Call save_manifest with a new path "
				    "afterwards to persist.",
				    DT::kMainThread);

				{
					Dia::Editor::EditorActionParam params[3];
					params[0].name="from"; params[0].type="string"; params[0].required=true; params[0].description="Source asset ID.";
					params[1].name="rel";  params[1].type="string"; params[1].required=true; params[1].description="Relationship label.";
					params[2].name="to";   params[2].type="string"; params[2].required=true; params[2].description="Target asset ID.";
					reg("asset_catalogue.add_relationship",
					    "Adds a directed relationship edge between two asset records. Runs AddRelationshipCommand "
					    "and auto-saves. The rel parameter names the relationship kind (e.g. 'uses', 'spawns').",
					    DT::kMainThread, params, 3);
				}

				{
					Dia::Editor::EditorActionParam params[3];
					params[0].name="from"; params[0].type="string"; params[0].required=true; params[0].description="Source asset ID.";
					params[1].name="rel";  params[1].type="string"; params[1].required=true; params[1].description="Relationship label.";
					params[2].name="to";   params[2].type="string"; params[2].required=true; params[2].description="Target asset ID.";
					reg("asset_catalogue.remove_relationship",
					    "Removes an existing directed relationship edge. Runs RemoveRelationshipCommand and auto-saves.",
					    DT::kMainThread, params, 3);
				}

				reg("asset_catalogue.browse_rules",
				    "Opens a file-picker dialog for the user to select a rules file, then loads it. "
				    "Requires an active window; do not call from headless scripts.",
				    DT::kMainThread);

				{
					Dia::Editor::EditorActionParam p;
					p.name="path"; p.type="string"; p.required=true;
					p.description="Absolute path to the rules file.";
					reg("asset_catalogue.load_rules",
					    "Loads an auto-categorisation rules file from the given path into the rules engine. "
					    "The rules are used by apply_rules to fill in type, status, and tag fields automatically.",
					    DT::kMainThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam params[2];
					params[0].name="excluded";          params[0].type="string[]"; params[0].required=false; params[0].description="Asset IDs to skip.";
					params[1].name="overwrite_manuals"; params[1].type="bool";     params[1].required=false; params[1].description="Overwrite manually-set fields (default: false).";
					reg("asset_catalogue.apply_rules",
					    "Applies all loaded rules to the catalogue. Records matching a rule's criteria have "
					    "their type, status, or tags updated automatically. Runs ApplyRulesCommand and "
					    "auto-saves. Excluded IDs are skipped.",
					    DT::kMainThread, params, 2);
				}

				{
					Dia::Editor::EditorActionParam params[2];
					params[0].name="assetType";        params[0].type="string"; params[0].required=true; params[0].description="Asset type ID, e.g. 'entity_template'.";
					params[1].name="editorPluginType"; params[1].type="string"; params[1].required=true; params[1].description="Plugin type ID that handles this asset type.";
					reg("asset_catalogue.register_type_editor",
					    "Registers a mapping from an asset type to the editor plugin that handles it. Called "
					    "by domain plugins at load time so that open_in_editor knows which plugin to activate.",
					    DT::kMainThread, params, 2);
				}

				{
					Dia::Editor::EditorActionParam params[3];
					params[0].name="id";          params[0].type="string";   params[0].required=true;  params[0].description="Unique scene asset ID.";
					params[1].name="source_path"; params[1].type="string";   params[1].required=true;  params[1].description="Relative path for the new .diascene file.";
					params[2].name="tags";        params[2].type="string[]"; params[2].required=false; params[2].description="Tag list.";
					reg("asset_catalogue.create_scene",
					    "Creates a new scene asset record and stub .diascene file, then loads DiaSceneEditor "
					    "to open it. Shorthand for create_asset with type='scene' plus a navigation step.",
					    DT::kMainThread, params, 3);
				}

				{
					Dia::Editor::EditorActionParam params[4];
					params[0].name="assetType";   params[0].type="string";   params[0].required=true;  params[0].description="Asset type ID.";
					params[1].name="id";          params[1].type="string";   params[1].required=true;  params[1].description="Unique asset identifier.";
					params[2].name="source_path"; params[2].type="string";   params[2].required=true;  params[2].description="Relative source file path.";
					params[3].name="tags";        params[3].type="string[]"; params[3].required=false; params[3].description="Tag list.";
					reg("asset_catalogue.create_asset",
					    "Creates a new asset record and writes the blank source file in one step. Runs "
					    "CreateRecordCommand and auto-saves.",
					    DT::kMainThread, params, 4);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="id"; p.type="string"; p.required=true;
					p.description="The asset ID to open.";
					reg("asset_catalogue.open_in_editor",
					    "Opens the given asset in its registered editor plugin — loads the plugin if not "
					    "already loaded, then navigates to the asset. Requires register_type_editor to have "
					    "been called for the asset's type.",
					    DT::kMainThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="id"; p.type="string"; p.required=true;
					p.description="The asset ID whose source file to open.";
					reg("asset_catalogue.open_in_file",
					    "Opens the asset's source file in the default OS application. Use to hand off to an "
					    "external editor. Requires an active desktop session.",
					    DT::kMainThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam params[6];
					params[0].name="id";          params[0].type="string";   params[0].required=true;  params[0].description="Unique asset identifier.";
					params[1].name="type";        params[1].type="string";   params[1].required=true;  params[1].description="Asset type ID.";
					params[2].name="source_path"; params[2].type="string";   params[2].required=true;  params[2].description="Relative path from manifest dir to the source file.";
					params[3].name="status";      params[3].type="string";   params[3].required=true;  params[3].description="Record status, e.g. 'active'.";
					params[4].name="scope";       params[4].type="string";   params[4].required=true;  params[4].description="Scope, e.g. 'project'.";
					params[5].name="tags";        params[5].type="string[]"; params[5].required=false; params[5].description="Tag list.";
					reg("asset_catalogue.create_record",
					    "Creates a new asset record in the catalogue without writing a source file. For types "
					    "that have an associated blank file a stub file is also written to source_path. Runs "
					    "CreateRecordCommand and auto-saves.",
					    DT::kMainThread, params, 6);
				}

				{
					Dia::Editor::EditorActionParam params[6];
					params[0].name="id";          params[0].type="string";   params[0].required=true;  params[0].description="Existing asset ID to update.";
					params[1].name="type";        params[1].type="string";   params[1].required=true;  params[1].description="Asset type ID.";
					params[2].name="source_path"; params[2].type="string";   params[2].required=true;  params[2].description="Relative source file path.";
					params[3].name="status";      params[3].type="string";   params[3].required=true;  params[3].description="Record status.";
					params[4].name="scope";       params[4].type="string";   params[4].required=true;  params[4].description="Scope.";
					params[5].name="tags";        params[5].type="string[]"; params[5].required=false; params[5].description="Tag list.";
					reg("asset_catalogue.update_record",
					    "Updates an existing asset record's fields. Recomputes content_hash if source_path "
					    "changes. Runs UpdateRecordCommand and auto-saves.",
					    DT::kMainThread, params, 6);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="id"; p.type="string"; p.required=true;
					p.description="The unique asset ID to delete.";
					reg("asset_catalogue.delete_record",
					    "Deletes an existing asset record from the catalogue. Runs DeleteRecordCommand and "
					    "auto-saves. Does not delete the source file on disk.",
					    DT::kMainThread, &p, 1);
				}

				{
					Dia::Editor::EditorActionParam p;
					p.name="records"; p.type="object[]"; p.required=true;
					p.description="Array of record descriptors; each has the same shape as create_record params.";
					reg("asset_catalogue.bulk_create_records",
					    "Creates multiple asset records in a single atomic command. Wraps N CreateRecordCommand "
					    "calls in a CompoundCommand so the entire batch is undone together. Auto-saves when done.",
					    DT::kMainThread, &p, 1);
				}

				reg("asset_catalogue.infer_relationships",
				    "Scans all .diascene files in the project, detects entity-template references, and "
				    "inserts the corresponding relationship edges into the catalogue. Idempotent — skips "
				    "edges that already exist. Auto-saves when done.",
				    DT::kMainThread);

				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: Dual-registered 34 asset_catalogue.* actions");
			}

		}
	}
}

using namespace Dia::AssetCatalogue::Editor;

REGISTER_EDITOR_PLUGIN(DiaAssetCatalogueEditorPlugin, "DiaAssetCatalogueEditorPlugin")
