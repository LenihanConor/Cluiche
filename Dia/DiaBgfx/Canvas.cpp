////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Canvas.h"
#include "DiaBgfx/Renderers/SpriteRenderer.h"
#include "DiaBgfx/Renderers/DebugRenderer.h"
#include "DiaBgfx/Renderers/UIOverlayRenderer.h"
#include "DiaBgfx/Resources/ShaderProgram.h"

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/EntityFrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/UIFrameData.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaImGui/DiaImGuiManager.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace Dia
{
    namespace Bgfx
    {
        // ---------- CanvasSettings ----------

        CanvasSettings::CanvasSettings()
            : Dia::Graphics::ICanvas::Settings()
            , rendererType(RendererType::Direct3D11)
            , initialSize(1400.0f, 1000.0f)
            , cookedShaderRoot("Cluiche/out/cluichetest/shaders")
        {}

        // ---------- bgfx backend name helpers ----------

        static const char* BackendSubdir(RendererType type)
        {
            switch (type)
            {
                case RendererType::Direct3D11: return "dx11";
                case RendererType::Direct3D12: return "dx12";
                case RendererType::Vulkan:     return "vulkan";
            }
            return "dx11";
        }

        static bgfx::RendererType::Enum ToBgfxRendererType(RendererType type)
        {
            switch (type)
            {
                case RendererType::Direct3D11: return bgfx::RendererType::Direct3D11;
                case RendererType::Direct3D12: return bgfx::RendererType::Direct3D12;
                case RendererType::Vulkan:     return bgfx::RendererType::Vulkan;
            }
            return bgfx::RendererType::Direct3D11;
        }

        // ---------- minimal bgfx callback handler ----------

        class CallbackHandler : public bgfx::CallbackI
        {
        public:
            void fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) override
            {
                DIA_LOG_ERROR("DiaBgfx", "bgfx fatal [%d] %s (%s:%u)", static_cast<int>(_code), _str, _filePath, _line);
                DIA_ASSERT(false, "bgfx fatal error — see log");
            }
            void traceVargs(const char* /*_filePath*/, uint16_t /*_line*/, const char* _format, va_list _argList) override
            {
                char buf[512];
                vsnprintf(buf, sizeof(buf), _format, _argList);
                DIA_LOG_DEBUG("DiaBgfx", "%s", buf);
            }
            void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
            void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
            void profilerEnd() override {}
            uint32_t cacheReadSize(uint64_t) override { return 0; }
            bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
            void cacheWrite(uint64_t, const void*, uint32_t) override {}
            void screenShot(const char*, uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, const void*, uint32_t, bool) override {}
            void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
            void captureEnd() override {}
            void captureFrame(const void*, uint32_t) override {}
        };

        static CallbackHandler sCallback;

        // ---------- Canvas ----------

        Canvas::Canvas()
            : mHwnd(nullptr)
            , mSize(0.0f, 0.0f)
            , mRendererType(RendererType::Direct3D11)
            , mInitialised(false)
            , mConfigured(false)
            , mShaderRoot(nullptr)
            , mSpriteProgram(nullptr)
            , mDebugProgram(nullptr)
            , mUIProgram(nullptr)
            , mSpriteRenderer(nullptr)
            , mDebugRenderer(nullptr)
            , mUIOverlayRenderer(nullptr)
        {}

        Canvas::~Canvas()
        {
            DIA_ASSERT(!mInitialised,
                "Canvas destroyed while still initialised — call SetActiveContext(false) first");
        }

        void Canvas::AttachToNativeWindow(Dia::Window::SystemHandle hwnd,
                                          const Dia::Maths::Vector2D& size)
        {
            DIA_ASSERT(!mInitialised, "Canvas::AttachToNativeWindow must be called before Initialize");
            mHwnd = hwnd;
            mSize = size;
        }

        void Canvas::Initialize(const Dia::Graphics::ICanvas::Settings& settings)
        {
            DIA_ASSERT(mHwnd != nullptr, "Canvas::Initialize — AttachToNativeWindow not called");
            DIA_ASSERT(!mConfigured, "Canvas::Initialize called twice");

            const CanvasSettings* cs = static_cast<const CanvasSettings*>(&settings);
            const RendererType rendererType = cs ? cs->rendererType : RendererType::Direct3D11;
            const Dia::Maths::Vector2D initSize = cs ? cs->initialSize : mSize;
            const char* shaderRoot = cs ? cs->cookedShaderRoot : "Cluiche/out/cluichetest/shaders";

            if (initSize.X() > 0.0f && initSize.Y() > 0.0f)
                mSize = initSize;

            mRendererType = rendererType;
            mShaderRoot = shaderRoot;
            mConfigured = true;

            DIA_LOG_INFO("DiaBgfx", "Canvas::Initialize — configured (%s, %.0fx%.0f), deferring bgfx::init to render thread",
                         BackendSubdir(rendererType), mSize.X(), mSize.Y());
        }

        void Canvas::DeferredInit()
        {
            DIA_ASSERT(mConfigured, "Canvas::DeferredInit — not configured");
            DIA_ASSERT(!mInitialised, "Canvas::DeferredInit — already initialised");

            DIA_LOG_INFO("DiaBgfx", "Canvas::DeferredInit — starting bgfx::init on render thread (hwnd=%p, %ux%u)",
                         mHwnd, static_cast<uint32_t>(mSize.X()), static_cast<uint32_t>(mSize.Y()));

            bgfx::Init init;
            init.type         = ToBgfxRendererType(mRendererType);
            init.platformData.nwh = mHwnd;
            init.resolution.width  = static_cast<uint32_t>(mSize.X());
            init.resolution.height = static_cast<uint32_t>(mSize.Y());
            init.resolution.reset  = BGFX_RESET_VSYNC;
            init.callback = &sCallback;

            DIA_LOG_INFO("DiaBgfx", "Canvas::DeferredInit — calling bgfx::init (type=%d)...",
                         static_cast<int>(init.type));

            if (!bgfx::init(init))
            {
                DIA_LOG_ERROR("DiaBgfx", "Canvas::DeferredInit — bgfx::init failed (renderer type %d)",
                              static_cast<int>(init.type));
                return;
            }

            DIA_LOG_INFO("DiaBgfx", "Canvas::DeferredInit — bgfx::init succeeded");

            bgfx::setViewClear(kEntityViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                               0x303030FFu, 1.0f, 0);
            bgfx::setViewClear(kDebugViewId,  BGFX_CLEAR_NONE);
            bgfx::setViewClear(kUIViewId,     BGFX_CLEAR_NONE);
            bgfx::setViewClear(kImGuiViewId,  BGFX_CLEAR_NONE);

            const char* backend = BackendSubdir(mRendererType);

            mSpriteProgram = new ShaderProgram();
            if (!mSpriteProgram->Load(mShaderRoot, backend, "sprite", "sprite"))
            {
                DIA_LOG_ERROR("DiaBgfx", "Canvas::DeferredInit — failed to load sprite shader");
            }

            mDebugProgram = new ShaderProgram();
            if (!mDebugProgram->Load(mShaderRoot, backend, "debug", "debug"))
            {
                DIA_LOG_ERROR("DiaBgfx", "Canvas::DeferredInit — failed to load debug shader");
            }

            mUIProgram = new ShaderProgram();
            if (!mUIProgram->Load(mShaderRoot, backend, "ui_overlay", "ui_overlay"))
            {
                DIA_LOG_ERROR("DiaBgfx", "Canvas::DeferredInit — failed to load ui_overlay shader");
            }

            mSpriteRenderer    = new SpriteRenderer(kEntityViewId, mSpriteProgram);
            mDebugRenderer     = new DebugRenderer(kDebugViewId, mDebugProgram);
            mUIOverlayRenderer = new UIOverlayRenderer(kUIViewId, mUIProgram);

            PropagateCanvasSize();

            mInitialised = true;
            DIA_LOG_INFO("DiaBgfx", "Canvas::DeferredInit complete (%s, %.0fx%.0f)",
                         backend, mSize.X(), mSize.Y());
        }

        void Canvas::SetCanvasSize(const Dia::Maths::Vector2D& size)
        {
            mSize = size;
            if (!mInitialised)
                return;
            const uint32_t w = static_cast<uint32_t>(size.X());
            const uint32_t h = static_cast<uint32_t>(size.Y());
            bgfx::reset(w, h, BGFX_RESET_VSYNC);
            PropagateCanvasSize();
        }

        void Canvas::SetActiveContext(bool active)
        {
            if (active && !mInitialised && mConfigured)
            {
                DeferredInit();
            }
            else if (!active && mInitialised)
            {
                // RenderPU is releasing us. Shut down on this thread (same
                // thread that called bgfx::init via DeferredInit).

                // ImGui renderer must be destroyed before bgfx::shutdown.
#ifdef DIA_DEBUG
                if (Dia::ImGui::GetManager().GetBackend() != nullptr)
                    Dia::ImGui::Shutdown();
#endif

                delete mUIOverlayRenderer;  mUIOverlayRenderer = nullptr;
                delete mDebugRenderer;      mDebugRenderer     = nullptr;
                delete mSpriteRenderer;     mSpriteRenderer    = nullptr;
                delete mUIProgram;          mUIProgram         = nullptr;
                delete mDebugProgram;       mDebugProgram      = nullptr;
                delete mSpriteProgram;      mSpriteProgram     = nullptr;

                bgfx::shutdown();
                mInitialised = false;
            }
        }

        void Canvas::StartFrame(const Dia::Graphics::FrameData& /*nextFrame*/)
        {
            if (!mInitialised)
            {
                if (mConfigured)
                    DeferredInit();
                if (!mInitialised)
                    return;
            }

            const uint16_t w = static_cast<uint16_t>(mSize.X());
            const uint16_t h = static_cast<uint16_t>(mSize.Y());

            bgfx::setViewRect(kEntityViewId, 0, 0, w, h);
            bgfx::setViewRect(kDebugViewId,  0, 0, w, h);
            bgfx::setViewRect(kUIViewId,     0, 0, w, h);
            bgfx::setViewRect(kImGuiViewId,  0, 0, w, h);

            bgfx::touch(kEntityViewId);
            bgfx::touch(kDebugViewId);
            bgfx::touch(kUIViewId);
            bgfx::touch(kImGuiViewId);
        }

        void Canvas::ProcessFrame(const Dia::Graphics::FrameData& nextFrame)
        {
            if (!mInitialised)
                return;

            if (mSpriteRenderer)
                mSpriteRenderer->Draw(static_cast<const Dia::Graphics::EntityFrameData&>(nextFrame));

            if (mDebugRenderer)
                mDebugRenderer->Draw(static_cast<const Dia::Graphics::DebugFrameData&>(nextFrame));

            if (mUIOverlayRenderer)
            {
                const Dia::Graphics::UIFrameData& uiFrame =
                    static_cast<const Dia::Graphics::UIFrameData&>(nextFrame);
                mUIOverlayRenderer->Composite(uiFrame.GetUIData());
            }
        }

        void Canvas::EndFrame(const Dia::Graphics::FrameData& /*nextFrame*/)
        {
            if (!mInitialised)
                return;

#ifdef DIA_DEBUG
            if (Dia::ImGui::GetManager().GetBackend() != nullptr)
                Dia::ImGui::Render();
#endif
            bgfx::frame();
        }

        Dia::UI::IUIRenderOverlay* Canvas::GetUIRenderOverlay()
        {
            return mUIOverlayRenderer;
        }

        void Canvas::PropagateCanvasSize()
        {
            if (mSpriteRenderer)
                mSpriteRenderer->OnCanvasSizeChanged(mSize);
            if (mDebugRenderer)
                mDebugRenderer->OnCanvasSizeChanged(mSize);
            if (mUIOverlayRenderer)
                mUIOverlayRenderer->OnCanvasSizeChanged(mSize);
        }

    } // namespace Bgfx
} // namespace Dia
