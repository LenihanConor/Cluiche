#include "DiaEditor/AppEditor/AppEditorController.h"
#include "DiaEditor/MVC/IEditorContext.h"
#include "DiaEditor/UI/WebUIBridge.h"
#include "DiaEditor/Project/ProjectContext.h"
#include "DiaEditor/Plugin/IPluginLoader.h"
#include <DiaObservation/Log/DiaLog.h>

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC AppEditorController::kUniqueId("AppEditorController");

		static const Dia::Core::StringCRC kReqGetActiveContext("app_editor.get_active_context");

		AppEditorController::AppEditorController()
			: mBridge(nullptr)
			, mContext(nullptr)
			, mFocusPluginId()
			, mFocusPanel{}
			, mEditTargetType()
			, mEditTargetId()
			, mEditTargetName{}
			, mEditTargetDirty(false)
			, mSelectionType()
			, mSelectionId()
			, mSelectionName{}
		{
		}

		void AppEditorController::Initialize(WebUIBridge* bridge, IEditorContext* context, IPluginLoader* pluginLoader)
		{
			mBridge       = bridge;
			mContext      = context;
			mPluginLoader = pluginLoader;

			if (mBridge == nullptr || mContext == nullptr)
				return;

			mBridge->RegisterRequestHandler(kReqGetActiveContext,
				[this](const Json::Value& d) { return HandleGetActiveContext(d); });
		}

		void AppEditorController::Shutdown()
		{
			if (mBridge != nullptr)
			{
				mBridge->UnregisterRequestHandler(kReqGetActiveContext);
				mBridge = nullptr;
			}
			mContext      = nullptr;
			mPluginLoader = nullptr;
		}

		void AppEditorController::SetFocus(Dia::Core::StringCRC pluginId, const char* panel)
		{
			mFocusPluginId = pluginId;
			if (panel != nullptr)
				strncpy_s(mFocusPanel, kMaxNameLength, panel, _TRUNCATE);
			else
				mFocusPanel[0] = '\0';
		}

		void AppEditorController::ClearFocus()
		{
			mFocusPluginId = Dia::Core::StringCRC();
			mFocusPanel[0] = '\0';
		}

		void AppEditorController::SetEditTarget(Dia::Core::StringCRC type, Dia::Core::StringCRC id, const char* name, bool dirty)
		{
			mEditTargetType  = type;
			mEditTargetId    = id;
			mEditTargetDirty = dirty;
			if (name != nullptr)
				strncpy_s(mEditTargetName, kMaxNameLength, name, _TRUNCATE);
			else
				mEditTargetName[0] = '\0';
		}

		void AppEditorController::SetDirty(bool dirty)
		{
			mEditTargetDirty = dirty;
		}

		void AppEditorController::SetSelection(Dia::Core::StringCRC type, Dia::Core::StringCRC id, const char* name)
		{
			mSelectionType = type;
			mSelectionId   = id;
			if (name != nullptr)
				strncpy_s(mSelectionName, kMaxNameLength, name, _TRUNCATE);
			else
				mSelectionName[0] = '\0';
		}

		void AppEditorController::ClearEditTarget()
		{
			mEditTargetType  = Dia::Core::StringCRC();
			mEditTargetId    = Dia::Core::StringCRC();
			mEditTargetName[0] = '\0';
			mEditTargetDirty = false;
		}

		void AppEditorController::ClearSelection()
		{
			mSelectionType   = Dia::Core::StringCRC();
			mSelectionId     = Dia::Core::StringCRC();
			mSelectionName[0] = '\0';
		}

		Json::Value AppEditorController::HandleGetActiveContext(const Json::Value& /*params*/)
		{
			Json::Value result;

			// project
			if (mContext != nullptr)
			{
				const ProjectContext& ctx = mContext->GetDiagameProject();
				if (ctx.IsValid())
				{
					// Derive id: last path segment without extension
					const char* lastSlash = ctx.diagamePath;
					for (const char* c = ctx.diagamePath; *c; ++c)
						if (*c == '/' || *c == '\\') lastSlash = c + 1;

					char id[64] = {0};
					unsigned int i = 0;
					while (lastSlash[i] && lastSlash[i] != '.' && i < sizeof(id) - 1)
					{
						id[i] = lastSlash[i];
						++i;
					}
					id[i] = '\0';

					Json::Value project;
					project["id"]    = id;
					project["state"] = "open";
					result["project"] = project;
				}
				else
				{
					result["project"] = Json::Value::null;
				}
			}
			else
			{
				result["project"] = Json::Value::null;
			}

			// focus
			if (mFocusPluginId == Dia::Core::StringCRC())
			{
				result["focus"] = Json::Value::null;
			}
			else
			{
				Json::Value focus;
				focus["plugin_id"] = mFocusPluginId.AsChar();
				focus["panel"]     = mFocusPanel;
				result["focus"] = focus;
			}

			// edit_target
			if (mEditTargetId == Dia::Core::StringCRC())
			{
				result["edit_target"] = Json::Value::null;
			}
			else
			{
				Json::Value editTarget;
				editTarget["type"]  = mEditTargetType.AsChar();
				editTarget["id"]    = mEditTargetId.AsChar();
				editTarget["name"]  = mEditTargetName;
				editTarget["dirty"] = mEditTargetDirty;
				result["edit_target"] = editTarget;
			}

			// selection
			if (mSelectionId == Dia::Core::StringCRC())
			{
				result["selection"] = Json::Value::null;
			}
			else
			{
				Json::Value selection;
				selection["type"] = mSelectionType.AsChar();
				selection["id"]   = mSelectionId.AsChar();
				selection["name"] = mSelectionName;
				result["selection"] = selection;
			}

			return result;
		}

		Json::Value AppEditorController::HandleNavigateTo(Dia::Core::StringCRC type, Dia::Core::StringCRC id)
		{
			// Check project open
			if (mContext == nullptr || !mContext->GetDiagameProject().IsValid())
			{
				DIA_LOG_WARNING("AppEditor", "app_editor.navigate_to: no project open");
				Json::Value r; r["success"] = false; r["reason"] = "no_project_open"; return r;
			}
			if (mBridge == nullptr)
			{
				Json::Value r; r["success"] = false; r["reason"] = "no_project_open"; return r;
			}

			// Empty id
			static const Dia::Core::StringCRC kEmpty;
			if (id == kEmpty)
			{
				DIA_LOG_WARNING("AppEditor", "app_editor.navigate_to: empty id");
				Json::Value r; r["success"] = false; r["reason"] = "not_found"; return r;
			}

			// Type-specific resolution
			if (type == Dia::Core::StringCRC("plugin"))
			{
				if (mPluginLoader == nullptr || !mPluginLoader->IsPluginTypeLoaded(id))
				{
					DIA_LOG_WARNING("AppEditor", "app_editor.navigate_to_plugin: plugin not loaded");
					Json::Value r; r["success"] = false; r["reason"] = "plugin_not_loaded"; return r;
				}
			}
			else if (type == Dia::Core::StringCRC("stage"))
			{
				// Stage validation deferred to JS for Phase 1:
				// full HasStage() check requires manifest query API not yet available.
			}
			else if (type == Dia::Core::StringCRC("entity"))
			{
				// Entity validation deferred to JS for Phase 1.
			}
			else if (type == Dia::Core::StringCRC("asset"))
			{
				DIA_LOG_WARNING("AppEditor", "app_editor.navigate_to_asset: not_implemented in Phase 1");
				Json::Value r; r["success"] = false; r["reason"] = "not_implemented"; return r;
			}

			// Push to JS
			Json::Value payload;
			payload["type"] = type.AsChar();
			payload["id"]   = id.AsChar();
			mBridge->NotifyUIDataChanged("app_editor.navigate_to", payload);

			DIA_LOG_INFO("AppEditor", "app_editor.navigate_to: navigating to %s '%s'", type.AsChar(), id.AsChar());

			Json::Value result;
			result["success"] = true;
			return result;
		}

	} // Editor
} // Dia
