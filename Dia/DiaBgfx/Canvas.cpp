////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Canvas.h"
#include "DiaBgfx/Capture/FrameCaptureRingBuffer.h"
#include "DiaBgfx/Renderers/SpriteRenderer.h"
#include "DiaBgfx/Renderers/DebugRenderer.h"
#include "DiaBgfx/Renderers/UIOverlayRenderer.h"
#include "DiaBgfx/Renderers/CellFieldRenderer.h"
#include "DiaBgfx/Renderers/CellFieldViewOrder.h"
#include "DiaBgfx/Resources/ShaderProgram.h"

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/EntityFrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/UIFrameData.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
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
            , cookedShaderRoot("assets/shaders")
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
            FrameCaptureRingBuffer* mRingBuffer = nullptr;

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
            void screenShot(const char* filePath, uint32_t width, uint32_t height, uint32_t pitch,
                            bgfx::TextureFormat::Enum /*format*/, const void* data, uint32_t size, bool yflip) override
            {
                if (mRingBuffer == nullptr) return;
                DIA_TRACE_ZONE("canvas.screenshot_callback", ::Dia::Observation::Trace::Category::kDiaGraphics);
                unsigned int slotIndex = FrameCaptureRingBuffer::DecodeSlotIndex(filePath);
                if (slotIndex >= FrameCaptureRingBuffer::kDepth) return;
                mRingBuffer->OnScreenShot(slotIndex, width, height, pitch, data, size, yflip);
            }
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
            , mCellFieldProgram(nullptr)
            , mSpriteRenderer(nullptr)
            , mDebugRenderer(nullptr)
            , mUIOverlayRenderer(nullptr)
            , mCellFieldRenderer(nullptr)
            , mCellFieldRegistry(nullptr)
            , mCaptureRingBuffer(nullptr)
        {}

        Canvas::~Canvas()
        {
            DIA_ASSERT(!mInitialised,
                "Canvas destroyed while still initialised — bgfx context should be shut down via DeferredInit before destruction");
        }

        void Canvas::SetCellFieldRegistry(Dia::Graphics::CellFieldRegistry* registry)
        {
            DIA_ASSERT(!mInitialised, "Canvas::SetCellFieldRegistry must be called before the first StartFrame");
            mCellFieldRegistry = registry;
        }

        void Canvas::SetCellFieldMetrics(const CellFieldMetrics& metrics)
        {
            mCellFieldObservation.SetMetrics(metrics);
        }

        bool Canvas::GetCellFieldRendererStats(CellFieldRendererStats& outStats) const
        {
            if (mCellFieldRenderer == nullptr)
            {
                outStats = CellFieldRendererStats();
                return false;
            }
            outStats = mCellFieldRenderer->GetRendererStats();
            return true;
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
            const char* shaderRoot = cs ? cs->cookedShaderRoot : "assets/shaders";

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

            bgfx::setDebug(BGFX_DEBUG_TEXT);

            bgfx::setViewClear(kEntityViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                               0x303030FFu, 1.0f, 0);
            bgfx::setViewClear(kDebugViewId,  BGFX_CLEAR_NONE);
            bgfx::setViewClear(kUIViewId,     BGFX_CLEAR_NONE);
            bgfx::setViewClear(kImGuiViewId,  BGFX_CLEAR_NONE);
            bgfx::setViewClear(kCellFieldViewId, BGFX_CLEAR_NONE);

            const char* backend = BackendSubdir(mRendererType);

            mSpriteProgram = new ShaderProgram();
            const bool spriteOk = mSpriteProgram->Load(mShaderRoot, backend, "sprite", "sprite");
            if (!spriteOk)
            {
                DIA_LOG_ERROR("DiaBgfx", "Canvas::DeferredInit — failed to load sprite shader");
            }

            mDebugProgram = new ShaderProgram();
            const bool debugOk = mDebugProgram->Load(mShaderRoot, backend, "debug", "debug");
            if (!debugOk)
            {
                DIA_LOG_ERROR("DiaBgfx", "Canvas::DeferredInit — failed to load debug shader");
            }

            mUIProgram = new ShaderProgram();
            const bool uiOk = mUIProgram->Load(mShaderRoot, backend, "ui_overlay", "ui_overlay");
            if (!uiOk)
            {
                DIA_LOG_ERROR("DiaBgfx", "Canvas::DeferredInit — failed to load ui_overlay shader");
            }

            if (!spriteOk || !debugOk || !uiOk)
            {
                DIA_LOG_WARNING("DiaBgfx", "Canvas::DeferredInit — one or more shaders missing (sprite=%s debug=%s ui=%s). "
                    "Rendering will be degraded. Run 'dia pipeline --target cluichetest' to cook shaders into '%s/%s/'.",
                    spriteOk ? "ok" : "MISSING",
                    debugOk  ? "ok" : "MISSING",
                    uiOk     ? "ok" : "MISSING",
                    mShaderRoot, backend);
            }

            mSpriteRenderer    = new SpriteRenderer(kEntityViewId, mSpriteProgram);
            mDebugRenderer     = new DebugRenderer(kDebugViewId, mDebugProgram);
            mUIOverlayRenderer = new UIOverlayRenderer(kUIViewId, mUIProgram);

            if (mCellFieldRegistry != nullptr)
            {
                // Shader attrs (TexCoord0/1/2 + Color0) are embedded in the cooked program; Load
                // takes no vertex layout. The geometry cache uses the matching layout.
                mCellFieldProgram = new ShaderProgram();
                if (!mCellFieldProgram->Load(mShaderRoot, backend, "cellfield", "cellfield"))
                {
                    DIA_LOG_WARNING("DiaBgfx", "Canvas::DeferredInit � failed to load cellfield shader; cell fields will not draw (run 'dia pipeline --target cluichetest')");
                }
                mCellFieldRenderer = new CellFieldRenderer(kCellFieldViewId, mCellFieldProgram, mCellFieldRegistry);
            }
            else
            {
                DIA_LOG_DEBUG("DiaBgfx", "Canvas::DeferredInit � no CellFieldRegistry set, CellField renderer skipped");
            }

            mCaptureRingBuffer = new FrameCaptureRingBuffer();
            sCallback.mRingBuffer = mCaptureRingBuffer;

            PropagateCanvasSize();

            mInitialised = true;
            DIA_LOG_INFO("DiaBgfx", "Canvas::DeferredInit complete (%s, %.0fx%.0f)",
                         backend, mSize.X(), mSize.Y());
        }

        void Canvas::Shutdown()
        {
            if (!mInitialised)
                return;

            if (mCellFieldRenderer != nullptr)
                mCellFieldRenderer->Shutdown();   // frees GPU objects while bgfx is still alive
            delete mCellFieldRenderer;  mCellFieldRenderer = nullptr;
            delete mUIOverlayRenderer;  mUIOverlayRenderer = nullptr;
            delete mDebugRenderer;      mDebugRenderer     = nullptr;
            delete mSpriteRenderer;     mSpriteRenderer    = nullptr;

            delete mCellFieldProgram; mCellFieldProgram = nullptr;
            delete mUIProgram;     mUIProgram     = nullptr;
            delete mDebugProgram;  mDebugProgram  = nullptr;
            delete mSpriteProgram; mSpriteProgram = nullptr;

            sCallback.mRingBuffer = nullptr;
            delete mCaptureRingBuffer;
            mCaptureRingBuffer = nullptr;

            bgfx::shutdown();
            mInitialised = false;

            DIA_LOG_INFO("DiaBgfx", "Canvas::Shutdown complete");
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

            if (mCellFieldRenderer != nullptr)
            {
                bgfx::setViewRect(kCellFieldViewId, 0, 0, w, h);
                // Entity -> cell field -> debug -> UI -> ImGui. Re-applied each frame (cheap) so a
                // bgfx::reset cannot drop it. Only views 4..8 are remapped; Canvas3D's 0-3 keep order.
                const auto order = BuildCellFieldViewRemap(kEntityViewId, kCellFieldViewId, kDebugViewId, kUIViewId, kImGuiViewId);
                bgfx::setViewOrder(kCellFieldViewOrderStart, static_cast<uint16_t>(kCellFieldViewOrderCount), order.data());
                bgfx::touch(kCellFieldViewId);
            }

            bgfx::touch(kEntityViewId);
            bgfx::touch(kDebugViewId);
            bgfx::touch(kUIViewId);
            bgfx::touch(kImGuiViewId);
        }

        void Canvas::ProcessFrame(const Dia::Graphics::FrameData& nextFrame)
        {
            if (!mInitialised)
                return;

            const Dia::Graphics::Camera2D& cam = nextFrame.GetCamera();
            if (mSpriteRenderer)
                mSpriteRenderer->SetCamera(cam);
            if (mDebugRenderer)
                mDebugRenderer->SetCamera(cam);
            if (mCellFieldRenderer)
                mCellFieldRenderer->SetCamera(cam);

            if (mSpriteRenderer)
                mSpriteRenderer->Draw(static_cast<const Dia::Graphics::EntityFrameData&>(nextFrame));

            if (mCellFieldRenderer)
                mCellFieldRenderer->Draw(static_cast<const Dia::Graphics::CellFieldFrameData&>(nextFrame));

            if (mCellFieldRegistry != nullptr)
            {
                CellFieldObservationInput observation;
                observation.registry = mCellFieldRegistry->GetStats();
                if (mCellFieldRenderer)
                    observation.renderer = mCellFieldRenderer->GetRendererStats();
                observation.registrySet = true;
                observation.programValid = mCellFieldRenderer != nullptr && mCellFieldProgram != nullptr && mCellFieldProgram->IsValid();
                mCellFieldObservation.Update(observation);
            }

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

        Dia::Graphics::FrameCaptureToken Canvas::RequestFrameCapture()
        {
            DIA_TRACE_ZONE("canvas.request_capture", ::Dia::Observation::Trace::Category::kDiaGraphics);
            if (!mInitialised || mCaptureRingBuffer == nullptr)
                return Dia::Graphics::FrameCaptureToken{};

            Dia::Graphics::FrameCaptureToken token = mCaptureRingBuffer->Claim();
            if (!token.IsValid())
            {
                DIA_LOG_WARNING("DiaBgfx", "Canvas::RequestFrameCapture — ring buffer full, dropping request");
                return token;
            }

            char slotPath[32];
            FrameCaptureRingBuffer::BuildSlotPath(FrameCaptureRingBuffer::SlotFromId(token.id), slotPath, sizeof(slotPath));
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, slotPath);
            return token;
        }

        Dia::Graphics::FrameCaptureResult Canvas::PollFrameCapture(const Dia::Graphics::FrameCaptureToken& token)
        {
            if (mCaptureRingBuffer == nullptr)
                return Dia::Graphics::FrameCaptureResult{};
            return mCaptureRingBuffer->Poll(token);
        }

        void Canvas::ReleaseFrameCapture(const Dia::Graphics::FrameCaptureToken& token)
        {
            if (mCaptureRingBuffer != nullptr)
                mCaptureRingBuffer->Release(token);
        }

        void Canvas::PropagateCanvasSize()
        {
            if (mSpriteRenderer)
                mSpriteRenderer->OnCanvasSizeChanged(mSize);
            if (mDebugRenderer)
                mDebugRenderer->OnCanvasSizeChanged(mSize);
            if (mUIOverlayRenderer)
                mUIOverlayRenderer->OnCanvasSizeChanged(mSize);
            if (mCellFieldRenderer)
                mCellFieldRenderer->OnCanvasSizeChanged(mSize);
        }

    } // namespace Bgfx
} // namespace Dia
