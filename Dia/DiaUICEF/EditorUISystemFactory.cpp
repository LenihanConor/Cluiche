////////////////////////////////////////////////////////////////////////////////
// Filename: EditorUISystemFactory.cpp
////////////////////////////////////////////////////////////////////////////////
#include "EditorUISystemFactory.h"
#include "CEFUISystem.h"

namespace Dia
{
	namespace UICEF
	{
		UI::IUISystem* CreateEditorUISystem(const Window::IWindow* window,
			const EditorUISystemConfig& config)
		{
			CEFUISystem* system = new CEFUISystem(window);
			system->SetSubprocessPath(config.subprocessPath);
			system->SetAssetBasePath(config.assetBasePath);
			system->SetWindowedRendering(config.windowedRendering);
			system->Initialize();
			return system;
		}

		void DestroyEditorUISystem(UI::IUISystem* system)
		{
			if (system)
			{
				system->Shutdown();
				delete system;
			}
		}
	}
}
