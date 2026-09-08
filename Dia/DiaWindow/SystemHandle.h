////////////////////////////////////////////////////////////////////////////////
// Filename: WindowHandle.h: Define a low-level window handle type, specific to
////////////////////////////////////////////////////////////////////////////////
#pragma once

#if defined(WIN32)
struct HWND__;
#endif

namespace Dia
{
	namespace Window
	{
#if defined(WIN32)

		// Window handle is HWND (HWND__*) on Windows
		typedef HWND__* SystemHandle;

#elif defined(ANDROID_OS)

		// Window handle is ANativeWindow (void*) on Android
		typedef void* SystemHandle;

#elif defined(__linux__)

		// Window handle is void* on Linux — covers both X11 (Window/unsigned long)
		// and Wayland (wl_surface*). SDL3 extracts the correct native handle
		// via SDL_GetPointerProperty at runtime.
		typedef void* SystemHandle;

#elif defined(__APPLE__) && defined(TARGET_OS_IOS) && TARGET_OS_IOS

		// Window handle is void* on iOS — wraps UIView*.
		// SDL3 extracts the UIView via SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER at runtime.
		typedef void* SystemHandle;

#endif
	}
}