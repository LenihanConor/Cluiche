////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaWindow/SystemHandle.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace UI { class IUIRenderOverlay; } }

namespace Dia
{
    namespace Bgfx
    {
        class FrameCaptureRingBuffer;
        class ShaderProgram;
        class SpriteRenderer;
        class DebugRenderer;
        class UIOverlayRenderer;

        enum class RendererType : unsigned char
        {
            Direct3D11 = 0,
            Direct3D12 = 1,
            Vulkan     = 2
        };

        struct CanvasSettings : public Dia::Graphics::ICanvas::Settings
        {
            CanvasSettings();

            RendererType         rendererType;
            Dia::Maths::Vector2D initialSize;
            const char*          cookedShaderRoot;  // e.g. "Cluiche/out/cluichetest/shaders"
        };

        // Implements ICanvas using bgfx. Owns three sub-renderers and three
        // ShaderProgram instances. Call AttachToNativeWindow before Initialize.
        // Per RB-004: implements ICanvas only — not IWindow or IInputSource.
        //
        // Threading: bgfx requires all API calls from the thread that called
        // bgfx::init(). Initialize() stores settings; actual bgfx::init() is
        // deferred to the first StartFrame() call (which runs on RenderPU).
        class Canvas : public Dia::Graphics::ICanvas
        {
        public:
            Canvas();
            ~Canvas() override;

            // Must be called BEFORE Initialize. Caller supplies the native window handle (HWND on Windows).
            void AttachToNativeWindow(Dia::Window::SystemHandle hwnd,
                                     const Dia::Maths::Vector2D& size);

            // ICanvas
            void Initialize(const Dia::Graphics::ICanvas::Settings& settings) override;
            void Shutdown() override;
            void SetCanvasSize(const Dia::Maths::Vector2D& size) override;
            void StartFrame(const Dia::Graphics::FrameData& nextFrame) override;
            void ProcessFrame(const Dia::Graphics::FrameData& nextFrame) override;
            void EndFrame(const Dia::Graphics::FrameData& nextFrame) override;
            Dia::Graphics::FrameCaptureToken RequestFrameCapture() override;
            Dia::Graphics::FrameCaptureResult PollFrameCapture(const Dia::Graphics::FrameCaptureToken& token) override;

            Dia::UI::IUIRenderOverlay* GetUIRenderOverlay();

            unsigned short GetImGuiViewId() const { return kImGuiViewId; }
            bool IsInitialised() const { return mInitialised; }

        private:
            void DeferredInit();
            void PropagateCanvasSize();

            Dia::Window::SystemHandle mHwnd;
            Dia::Maths::Vector2D      mSize;
            RendererType              mRendererType;
            bool                      mInitialised;
            bool                      mConfigured;

            const char*               mShaderRoot;

            static constexpr unsigned short kEntityViewId = 0;
            static constexpr unsigned short kDebugViewId  = 1;
            static constexpr unsigned short kUIViewId     = 2;
            static constexpr unsigned short kImGuiViewId  = 3;

            ShaderProgram*     mSpriteProgram;    // owned
            ShaderProgram*     mDebugProgram;     // owned
            ShaderProgram*     mUIProgram;        // owned

            SpriteRenderer*    mSpriteRenderer;   // owned
            DebugRenderer*     mDebugRenderer;    // owned
            UIOverlayRenderer* mUIOverlayRenderer; // owned

            FrameCaptureRingBuffer* mCaptureRingBuffer; // owned

            Canvas(const Canvas&) = delete;
            Canvas& operator=(const Canvas&) = delete;
        };

    } // namespace Bgfx
} // namespace Dia
