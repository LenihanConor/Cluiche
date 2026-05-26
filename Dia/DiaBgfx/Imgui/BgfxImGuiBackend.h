////////////////////////////////////////////////////////////////////////////////
// Filename: BgfxImGuiBackend.h
// Feature spec: docs/specs/features/dia/diabgfx/imgui-backend.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaImGui/IImGuiBackend.h>
#include <DiaWindow/SystemHandle.h>

namespace Dia
{
    namespace Bgfx
    {
        class BgfxImGuiBackend : public Dia::ImGui::IImGuiBackend
        {
        public:
            BgfxImGuiBackend();
            ~BgfxImGuiBackend() override;

            // Call before Init. viewId must be the last bgfx view (drawn on top).
            // hwnd is the Win32 window handle (from DiaSFML::Window::GetSystemHandle()).
            void Configure(unsigned short viewId, Dia::Window::SystemHandle hwnd);

            // IImGuiBackend
            void Init()             override;
            void Shutdown()         override;
            void NewFrame(float dt) override;
            void Render()           override;

            // Win32 message pump hook — install via Win32WndProcChain.
            // Returns true if ImGui consumed the message.
            static bool WndProcThunk(void* hwnd, unsigned int msg,
                                     unsigned long long wparam, long long lparam,
                                     void* user);

        private:
            Dia::Window::SystemHandle mHwnd;
            unsigned short            mViewId;
            bool                      mInitialised;
        };

    } // namespace Bgfx
} // namespace Dia

#endif // DIA_DEBUG
