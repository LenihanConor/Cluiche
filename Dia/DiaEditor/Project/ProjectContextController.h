#pragma once

#include <DiaEditor/Project/ProjectContext.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class IEditorContext;
		class WebUIBridge;
		class EditorActionRegistry;

		// Registers project.* WebUIBridge request handlers and pushes
		// "project_changed" topic updates when the game-project context changes.
		// If api is non-null, also dual-registers the P0 actions via EditorActionRegistry
		// so they are reachable from Python/DiaAPI in addition to the JS path.
		class ProjectContextController
		{
		public:
			ProjectContextController();

			void Initialize(WebUIBridge* bridge, IEditorContext* context,
			                EditorActionRegistry* api = nullptr);
			void Shutdown();

		private:
			Json::Value HandleOpenPath(const Json::Value& data);
			Json::Value HandleClose(const Json::Value& data);
			Json::Value HandleGetRecent(const Json::Value& data);
			Json::Value HandleOpen(const Json::Value& data);
			Json::Value HandleGetState(const Json::Value& data);

			void PushProjectChanged();

			static void OnProjectChangedStatic(const ProjectContext& ctx, void* ud);

			WebUIBridge*          mBridge;
			IEditorContext*       mContext;
			EditorActionRegistry* mApi = nullptr;
		};
	}
}
