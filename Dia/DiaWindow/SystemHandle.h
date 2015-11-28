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

#endif
	}
}