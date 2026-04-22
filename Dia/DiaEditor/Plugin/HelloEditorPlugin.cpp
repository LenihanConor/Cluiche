#include "DiaEditor/Plugin/HelloEditorPlugin.h"
#include "DiaEditor/Plugin/EditorPluginRegistrationMacros.h"

#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace Editor
	{
		HelloEditorPlugin::HelloEditorPlugin()
			: mLoaded(false)
		{}

		void HelloEditorPlugin::OnLoad(const EditorPluginContext& /*context*/)
		{
			mLoaded = true;
			DIA_LOG_INFO("Editor", "HelloEditorPlugin: OnLoad");
		}

		void HelloEditorPlugin::OnUnload()
		{
			mLoaded = false;
			DIA_LOG_INFO("Editor", "HelloEditorPlugin: OnUnload");
		}

		void HelloEditorPlugin::OnUpdate(float /*deltaTime*/)
		{
		}
	}
}

using namespace Dia::Editor;

REGISTER_EDITOR_PLUGIN(HelloEditorPlugin, "HelloEditorPlugin")
