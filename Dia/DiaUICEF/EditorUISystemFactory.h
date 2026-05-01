////////////////////////////////////////////////////////////////////////////////
// Filename: EditorUISystemFactory.h
// Creates an IUISystem configured for hosting a web-based editor UI in a
// native window. Callers get an IUISystem* and never touch CEF types.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaUI/IUISystem.h>

namespace Dia
{
	namespace Window
	{
		class IWindow;
	}

	namespace UICEF
	{
		struct EditorUISystemConfig
		{
			// Path to the subprocess exe (may be the host exe itself if it
			// handles CefExecuteProcess at startup). Relative paths are
			// resolved against the host exe directory.
			const char* subprocessPath = "";

			// Root directory for dia:// scheme lookups. Empty = exe directory.
			const char* assetBasePath = "";

			// If true, CEF paints into a child window of the host IWindow.
			// If false, CEF renders offscreen into a texture buffer.
			bool windowedRendering = true;
		};

		UI::IUISystem* CreateEditorUISystem(const Window::IWindow* window,
			const EditorUISystemConfig& config);

		void DestroyEditorUISystem(UI::IUISystem* system);
	}
}
