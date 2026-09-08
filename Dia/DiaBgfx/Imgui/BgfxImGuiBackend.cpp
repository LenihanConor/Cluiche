////////////////////////////////////////////////////////////////////////////////
// Filename: BgfxImGuiBackend.cpp
// Feature spec: docs/specs/features/dia/diabgfx/imgui-backend.md
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Imgui/BgfxImGuiBackend.h"

#ifdef DIA_DEBUG

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "DiaBgfx/Canvas.h"
#include "DiaBgfx/Imgui/BgfxImGuiRenderer.h"
#include <DiaBgfx/Imgui/Win32WndProcChain.h>

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
            , mCanvas(nullptr)
            , mViewId(255)
            , mInitialised(false)
            , mRendererCreated(false)
        {}

        BgfxImGuiBackend::~BgfxImGuiBackend()
        {
            if (mInitialised)
                Shutdown();
        }

        void BgfxImGuiBackend::Configure(unsigned short viewId, Dia::Window::SystemHandle hwnd, Canvas* canvas)
        {
            mViewId = viewId;
            mHwnd   = hwnd;
            mCanvas = canvas;
        }

        void BgfxImGuiBackend::Init()
        {
            if (mInitialised)
                return;

            if (::ImGui::GetCurrentContext() == nullptr)
                ::ImGui::CreateContext();

            ::ImGui::GetIO().IniFilename = "assets/config/imgui.ini";

            ImGui_ImplWin32_Init(reinterpret_cast<HWND>(mHwnd));

            Dia::Bgfx::Win32WndProcChain::Install(mHwnd, &BgfxImGuiBackend::WndProcThunk, this);

            // imguiCreate() deferred to first NewFrame — requires bgfx to be initialised,
            // which happens on RenderPU's thread via Canvas::DeferredInit().
            mInitialised = true;
        }

        void BgfxImGuiBackend::Shutdown()
        {
            if (!mInitialised)
                return;

            Dia::Bgfx::Win32WndProcChain::Uninstall(mHwnd);
            if (mRendererCreated)
                imguiDestroy();
            ImGui_ImplWin32_Shutdown();
            mRendererCreated = false;
            mInitialised = false;
        }

        void BgfxImGuiBackend::NewFrame(float /*dt*/)
        {
            if (!mInitialised)
                return;

            if (!mCanvas || !mCanvas->IsInitialised())
                return;

            if (!mRendererCreated)
            {
                imguiCreate(18.0f);
                mRendererCreated = true;
            }

            ImGui_ImplWin32_NewFrame();

            ImGuiIO& io = ::ImGui::GetIO();
            imguiBeginFrameNoInput(
                (uint16_t)io.DisplaySize.x,
                (uint16_t)io.DisplaySize.y,
                mViewId);
        }

        void BgfxImGuiBackend::Render()
        {
            if (!mRendererCreated)
                return;

            imguiEndFrame();
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
