////////////////////////////////////////////////////////////////////////////////
// Filename: Win32WndProcChain.cpp
// Feature spec: docs/specs/features/dia/diabgfx/imgui-backend.md
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/Win32WndProcChain.h"

#ifdef DIA_DEBUG

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Dia
{
    namespace SFML
    {
        static WNDPROC                     g_originalWndProc = nullptr;
        static Win32WndProcChain::PreHandler g_preHandler     = nullptr;
        static void*                       g_preUser         = nullptr;

        static LRESULT CALLBACK ChainProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
        {
            if (g_preHandler &&
                g_preHandler(hwnd, static_cast<unsigned int>(msg),
                             static_cast<unsigned long long>(w),
                             static_cast<long long>(l), g_preUser))
            {
                return 0;  // consumed by pre-handler
            }
            return CallWindowProc(g_originalWndProc, hwnd, msg, w, l);
        }

        void Win32WndProcChain::Install(Dia::Window::SystemHandle hwnd, PreHandler handler, void* user)
        {
            g_preHandler = handler;
            g_preUser    = user;
            g_originalWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtr(static_cast<HWND>(hwnd), GWLP_WNDPROC,
                                 reinterpret_cast<LONG_PTR>(ChainProc)));
        }

        void Win32WndProcChain::Uninstall(Dia::Window::SystemHandle hwnd)
        {
            if (g_originalWndProc)
            {
                SetWindowLongPtr(static_cast<HWND>(hwnd), GWLP_WNDPROC,
                                 reinterpret_cast<LONG_PTR>(g_originalWndProc));
            }
            g_originalWndProc = nullptr;
            g_preHandler      = nullptr;
            g_preUser         = nullptr;
        }

    } // namespace SFML
} // namespace Dia

#endif // DIA_DEBUG
