#include "DiaEditor/Plugin/GameConnectionEditorPlugin.h"
#include "DiaEditor/Plugin/EditorPluginRegistrationMacros.h"
#include "DiaEditor/Plugin/EditorPluginContext.h"

#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace Editor
	{
		void GameConnectionEditorPlugin::OnLoad(const EditorPluginContext& context)
		{
			DIA_LOG_INFO("Editor", "GameConnectionEditorPlugin: OnLoad");
			mManager.Initialize();
			mController.SetPersistencePath("assets/configs/editor-connection.json");
			mController.LoadPersistedUrl();
			mController.Initialize(context.mBridge, &mManager, context.mView);
			mController.AutoConnect("ws://localhost:9002");
			DIA_LOG_INFO("Editor", "GameConnectionEditorPlugin: Initialized manager and controller");
		}

		void GameConnectionEditorPlugin::OnUnload()
		{
			DIA_LOG_INFO("Editor", "GameConnectionEditorPlugin: OnUnload");
			mController.Shutdown();
			mManager.Shutdown();
		}

		void GameConnectionEditorPlugin::OnUpdate(float deltaTime)
		{
			mManager.Update(deltaTime);
			mController.Update(deltaTime);
		}
	}
}

using namespace Dia::Editor;

REGISTER_EDITOR_PLUGIN(GameConnectionEditorPlugin, "GameConnectionEditorPlugin")
