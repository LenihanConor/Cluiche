////////////////////////////////////////////////////////////////////////////////
// Filename: Dia/DiaBgfx/Imgui/Win32WndProcChain.h
// Description: Installs a Win32 WndProc pre-handler on a window so that
//              DIA_DEBUG-only consumers (e.g. BgfxImGuiBackend) can intercept
//              Win32 messages before the window library processes them.
// Feature spec: docs/specs/features/dia/diabgfx/imgui-backend.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaWindow/SystemHandle.h>

namespace Dia
{
    namespace Bgfx
    {
        class Win32WndProcChain
        {
        public:
            // Pre-handler signature. Return true if the message was consumed
            // (caller must NOT forward it to the original WndProc).
            using PreHandler = bool (*)(void* hwnd, unsigned int msg,
                                        unsigned long long wparam, long long lparam,
                                        void* user);

            // Install a pre-handler on hwnd. Only one handler is supported at a time.
            static void Install(Dia::Window::SystemHandle hwnd, PreHandler handler, void* user);

            // Restore the original WndProc and clear the pre-handler.
            static void Uninstall(Dia::Window::SystemHandle hwnd);
        };

    } // namespace Bgfx
} // namespace Dia

#endif // DIA_DEBUG
