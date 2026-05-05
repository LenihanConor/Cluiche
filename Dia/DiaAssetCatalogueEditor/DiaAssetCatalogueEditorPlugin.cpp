#include "DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h"
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
		}
	}
}

using namespace Dia::AssetCatalogue::Editor;

REGISTER_EDITOR_PLUGIN(DiaAssetCatalogueEditorPlugin, "DiaAssetCatalogueEditorPlugin")
