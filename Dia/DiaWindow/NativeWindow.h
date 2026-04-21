////////////////////////////////////////////////////////////////////////////////
// Filename: NativeWindow.h
// Platform-neutral factory for the host OS window. Callers receive an IWindow*
// and never need to know about Win32Window, HWND, or <windows.h>.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaWindow/Interface/IWindow.h>

#include <functional>

namespace Dia
{
	namespace Window
	{
		using WindowCloseCallback = std::function<void()>;

		IWindow* CreateNativeWindow(const IWindow::Settings& settings,
			WindowCloseCallback onClose = WindowCloseCallback());

		void DestroyNativeWindow(IWindow* window);

		// Drain any pending OS events for the given window.
		void PumpNativeMessages(IWindow* window);
	}
}
