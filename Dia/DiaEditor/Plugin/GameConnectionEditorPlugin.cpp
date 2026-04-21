#include "DiaEditor/Plugin/GameConnectionEditorPlugin.h"
#include "DiaEditor/Plugin/EditorPluginRegistrationMacros.h"
#include "DiaEditor/Plugin/EditorPluginContext.h"

#include <DiaCore/Core/Log.h>

namespace Dia
{
	namespace Editor
	{
		void GameConnectionEditorPlugin::OnLoad(const EditorPluginContext& context)
		{
			Dia::Core::Log::OutputVaradicLine("GameConnectionEditorPlugin: OnLoad");
			mManager.Initialize();
			mController.SetPersistencePath("Data/editor-connection.json");
			mController.LoadPersistedUrl();
			mController.Initialize(context.mBridge, &mManager, context.mView);
			Dia::Core::Log::OutputVaradicLine("GameConnectionEditorPlugin: Initialized manager and controller");
		}

		void GameConnectionEditorPlugin::OnUnload()
		{
			Dia::Core::Log::OutputVaradicLine("GameConnectionEditorPlugin: OnUnload");
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
