////////////////////////////////////////////////////////////////////////////////
// Filename: BgfxImGuiBackend.cpp
// Feature spec: docs/specs/features/dia/diabgfx/imgui-backend.md
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Imgui/BgfxImGuiBackend.h"

#ifdef DIA_DEBUG

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "DiaBgfx/Imgui/BgfxImGuiRenderer.h"
#include <DiaSFML/Win32WndProcChain.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>

// WndProcHandler is guarded by #if 0 in imgui_impl_win32.h to avoid pulling <windows.h>;
// forward-declare it here after <windows.h> is already included (per imgui guidance).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Dia
{
    namespace Bgfx
    {
        BgfxImGuiBackend::BgfxImGuiBackend()
            : mHwnd(nullptr)
            , mViewId(255)
            , mInitialised(false)
        {}

        BgfxImGuiBackend::~BgfxImGuiBackend()
        {
            if (mInitialised)
                Shutdown();
        }

        void BgfxImGuiBackend::Configure(unsigned short viewId, Dia::Window::SystemHandle hwnd)
        {
            mViewId = viewId;
            mHwnd   = hwnd;
        }

        void BgfxImGuiBackend::Init()
        {
            if (mInitialised)
                return;

            if (::ImGui::GetCurrentContext() == nullptr)
                ::ImGui::CreateContext();

            ImGui_ImplWin32_Init(reinterpret_cast<HWND>(mHwnd));

            imguiCreate(18.0f);

            Dia::SFML::Win32WndProcChain::Install(mHwnd, &BgfxImGuiBackend::WndProcThunk, this);

            mInitialised = true;
        }

        void BgfxImGuiBackend::Shutdown()
        {
            if (!mInitialised)
                return;

            Dia::SFML::Win32WndProcChain::Uninstall(mHwnd);
            imguiDestroy();
            ImGui_ImplWin32_Shutdown();
            // DiaImGuiManager owns the context; do not destroy it here.
            mInitialised = false;
        }

        void BgfxImGuiBackend::NewFrame(float /*dt*/)
        {
            if (!mInitialised)
                return;

            ImGui_ImplWin32_NewFrame();

            ImGuiIO& io = ::ImGui::GetIO();
            imguiBeginFrame(
                (int32_t)io.MousePos.x,
                (int32_t)io.MousePos.y,
                (io.MouseDown[0] ? 0x01u : 0u) |
                (io.MouseDown[1] ? 0x02u : 0u) |
                (io.MouseDown[2] ? 0x04u : 0u),
                0,          // scroll handled by imgui_impl_win32
                (uint16_t)io.DisplaySize.x,
                (uint16_t)io.DisplaySize.y,
                -1,
                mViewId);
            // imguiBeginFrame calls ImGui::NewFrame() internally
        }

        void BgfxImGuiBackend::Render()
        {
            if (!mInitialised)
                return;

            imguiEndFrame();  // calls ImGui::Render() + bgfx draw submission
        }

        /*static*/ bool BgfxImGuiBackend::WndProcThunk(void* hwnd, unsigned int msg,
                                                        unsigned long long wparam, long long lparam,
                                                        void* /*user*/)
        {
            return ImGui_ImplWin32_WndProcHandler(
                static_cast<HWND>(hwnd),
                static_cast<UINT>(msg),
                static_cast<WPARAM>(wparam),
                static_cast<LPARAM>(lparam)) != 0;
        }

    } // namespace Bgfx
} // namespace Dia

#endif // DIA_DEBUG
