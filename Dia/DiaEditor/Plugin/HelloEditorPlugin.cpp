#include "DiaEditor/Plugin/HelloEditorPlugin.h"
#include "DiaEditor/Plugin/EditorPluginRegistrationMacros.h"

#include <DiaCore/Core/Log.h>

namespace Dia
{
	namespace Editor
	{
		HelloEditorPlugin::HelloEditorPlugin()
			: mLoaded(false)
		{}

		void HelloEditorPlugin::OnLoad(EditorModel* /*model*/)
		{
			mLoaded = true;
			Dia::Core::Log::OutputVaradicLine("HelloEditorPlugin: OnLoad");
		}

		void HelloEditorPlugin::OnUnload()
		{
			mLoaded = false;
			Dia::Core::Log::OutputVaradicLine("HelloEditorPlugin: OnUnload");
		}

		void HelloEditorPlugin::OnUpdate(float /*deltaTime*/)
		{
		}
	}
}

using namespace Dia::Editor;

REGISTER_EDITOR_PLUGIN(HelloEditorPlugin, "HelloEditorPlugin")
