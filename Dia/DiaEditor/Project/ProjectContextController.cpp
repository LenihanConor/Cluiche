#include "DiaEditor/Project/ProjectContextController.h"
#include "DiaEditor/MVC/IEditorContext.h"
#include "DiaEditor/UI/WebUIBridge.h"
#include "DiaEditor/UI/DataPath.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/CRC/StringCRC.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

namespace Dia
{
	namespace Editor
	{
		static const Dia::Core::StringCRC kReqOpenPath("project.open_path");
		static const Dia::Core::StringCRC kReqClose("project.close");
		static const Dia::Core::StringCRC kReqGetRecent("project.get_recent");
		static const Dia::Core::StringCRC kReqOpen("project.open");

		ProjectContextController::ProjectContextController()
			: mBridge(nullptr)
			, mContext(nullptr)
		{
		}

		void ProjectContextController::Initialize(WebUIBridge* bridge, IEditorContext* context)
		{
			mBridge  = bridge;
			mContext = context;

			if (mBridge == nullptr || mContext == nullptr)
				return;

			mBridge->RegisterRequestHandler(kReqOpenPath, [this](const Json::Value& d) { return HandleOpenPath(d); });
			mBridge->RegisterRequestHandler(kReqClose,    [this](const Json::Value& d) { return HandleClose(d); });
			mBridge->RegisterRequestHandler(kReqGetRecent,[this](const Json::Value& d) { return HandleGetRecent(d); });
			mBridge->RegisterRequestHandler(kReqOpen,     [this](const Json::Value& d) { return HandleOpen(d); });

			mContext->OnDiagameProjectChanged(&ProjectContextController::OnProjectChangedStatic, this);

			PushProjectChanged();
		}

		void ProjectContextController::Shutdown()
		{
			if (mBridge != nullptr)
			{
				mBridge->UnregisterRequestHandler(kReqOpenPath);
				mBridge->UnregisterRequestHandler(kReqClose);
				mBridge->UnregisterRequestHandler(kReqGetRecent);
				mBridge->UnregisterRequestHandler(kReqOpen);
				mBridge = nullptr;
			}
			mContext = nullptr;
		}

		Json::Value ProjectContextController::HandleOpenPath(const Json::Value& data)
		{
			Json::Value result;
			if (!data.isMember("path") || !data["path"].isString())
			{
				result["ok"]    = false;
				result["error"] = "missing path";
				return result;
			}
			const char* path = data["path"].asCString();
			if (!mContext->LoadDiagameProject(path))
			{
				result["ok"]    = false;
				result["error"] = "failed to load project";
				return result;
			}
			result["ok"]   = true;
			result["path"] = path;
			return result;
		}

		Json::Value ProjectContextController::HandleClose(const Json::Value& /*data*/)
		{
			if (mContext != nullptr)
				mContext->ClearDiagameProject();
			Json::Value result;
			result["ok"] = true;
			return result;
		}

		Json::Value ProjectContextController::HandleGetRecent(const Json::Value& /*data*/)
		{
			Json::Value result;
			Json::Value paths(Json::arrayValue);
			if (mContext != nullptr)
			{
				unsigned int count = mContext->GetRecentProjectCount();
				for (unsigned int i = 0; i < count; ++i)
				{
					const char* p = mContext->GetRecentProject(i);
					if (p != nullptr && p[0] != '\0')
						paths.append(p);
				}
			}
			result["paths"] = paths;
			return result;
		}

		Json::Value ProjectContextController::HandleOpen(const Json::Value& /*data*/)
		{
			Json::Value result;

			char filePath[MAX_PATH] = {0};
			OPENFILENAMEA ofn = {};
			ofn.lStructSize  = sizeof(ofn);
			ofn.hwndOwner    = nullptr;
			ofn.lpstrFilter  = "Dia Game Files (*.diagame)\0*.diagame\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile    = filePath;
			ofn.nMaxFile     = MAX_PATH;
			ofn.lpstrTitle   = "Open .diagame";
			ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (!GetOpenFileNameA(&ofn))
			{
				result["ok"]       = false;
				result["cancelled"] = true;
				return result;
			}

			if (mContext == nullptr || !mContext->LoadDiagameProject(filePath))
			{
				result["ok"]    = false;
				result["error"] = "failed to load project";
				return result;
			}

			result["ok"]   = true;
			result["path"] = filePath;
			return result;
		}

		void ProjectContextController::PushProjectChanged()
		{
			if (mBridge == nullptr || mContext == nullptr)
				return;

			const ProjectContext& ctx = mContext->GetDiagameProject();

			Json::Value payload;
			if (ctx.IsValid())
			{
				// Extract project name: last path segment without extension.
				const char* lastSlash = ctx.diagamePath;
				for (const char* c = ctx.diagamePath; *c; ++c)
					if (*c == '/' || *c == '\\') lastSlash = c + 1;

				char name[64] = {0};
				unsigned int i = 0;
				while (lastSlash[i] && lastSlash[i] != '.' && i < sizeof(name) - 1)
				{
					name[i] = lastSlash[i];
					++i;
				}
				name[i] = '\0';

				payload["name"]        = name;
				payload["diagamePath"] = ctx.diagamePath;
				payload["source"]      = (ctx.source == ProjectSource::kLive) ? "live" : "manual";
			}
			else
			{
				payload["name"]        = "";
				payload["diagamePath"] = "";
				payload["source"]      = "manual";
			}

			mBridge->NotifyUIDataChanged(DataPath::kProjectChanged.AsChar(), payload);
		}

		void ProjectContextController::OnProjectChangedStatic(const ProjectContext& /*ctx*/, void* ud)
		{
			auto* self = static_cast<ProjectContextController*>(ud);
			self->PushProjectChanged();
		}
	}
}
