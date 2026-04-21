////////////////////////////////////////////////////////////////////////////////
// Filename: NativeWindow.cpp
////////////////////////////////////////////////////////////////////////////////
#include "NativeWindow.h"
#include "Win32Window.h"

namespace Dia
{
	namespace Window
	{
		IWindow* CreateNativeWindow(const IWindow::Settings& settings,
			WindowCloseCallback onClose)
		{
			Win32Window* window = new Win32Window();
			if (onClose)
				window->SetCloseCallback(onClose);
			window->Initialize(settings);
			return window;
		}

		void SetNativeResizeCallback(IWindow* window, WindowResizeCallback onResize)
		{
			if (Win32Window* w = static_cast<Win32Window*>(window))
				w->SetResizeCallback(onResize);
		}

		void DestroyNativeWindow(IWindow* window)
		{
			delete window;
		}

		void PumpNativeMessages(IWindow* window)
		{
			if (Win32Window* w = static_cast<Win32Window*>(window))
				w->PumpMessages();
		}
	}
}
