#include "Modules/RenderModule.h"
#include "Modules/KernelModule.h"

#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaBgfx3D/Canvas3D.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Session/SessionManager.h>
#include <DiaObservation/Capture/CaptureManager.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

#ifdef DIA_DEBUG
#include <DiaImGui/DiaImGuiManager.h>
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC RenderModule::kTypeId("RenderModule");

RenderModule::RenderModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult RenderModule::DoStart()
{
    // RenderModule runs on the RenderPU dedicated thread, KernelModule runs on
    // MainPU. Wait (return kLoading) until the KernelCanvas service stream is
    // committed (framework commits it after KernelModule's DoStart registers it).
    if (!mCanvasService.IsAvailable())
    {
        return Dia::ApplicationFlow::StartResult::kLoading;
    }

    mCanvas = &mCanvasService.Get();

    // Attempt to get Canvas3D — valid when KernelModule created a Canvas3D (3D rendering enabled).
    mCanvas3D = static_cast<Dia::Bgfx3D::Canvas3D*>(mCanvas);

    if (mCanvas3D && mMeshHandlerService.IsAvailable())
        mCanvas3D->SetMeshHandler(&mMeshHandlerService.Get());

    DIA_LOG_INFO("Application", "RenderModule DoStart: canvas acquired");

    // Reset the cross-PU stop fence on every (re-)entry. KernelModule::DoStop
    // blocks until this flips back to true in our DoStop.
    KernelModule::SetRenderContextReleased(false);

    // Signal to DebugUIModule (same PU) that the render context is ready for ImGui.
    KernelModule::SetRenderContextActive(true);

    // VSync is configured via ICanvas::Settings at initialization time (in KernelModule).
    // No runtime SetVSync API exists on ICanvas; VSyncEnum::kEnable is the default.

    // Register metrics for cross-thread UI synchronization telemetry
    auto& metricReg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricUIPreserved)
        mMetricUIPreserved = metricReg.RegisterCounter(Dia::Core::StringCRC("render.ui_preserved"));

    DIA_LOG_INFO("Application", "RenderModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void RenderModule::DoUpdate(float /*dt*/)
{
    if (mCanvas == nullptr)
        return;

    // Issue any queued screen captures before the frame render (bgfx API thread required).
    auto* cm = Dia::Observation::SessionManager::GetActiveCaptureManager();
    if (cm) cm->RenderTick();

    // Try to fetch a 3D frame first.
    // Guard on GetMeshDraws().Size() > 0: FrameStreamStore never clears hasData,
    // so after a 3D stage stops it writes a cleared flush frame (0 draws) and
    // FetchLatest() keeps returning it forever.  Without this guard rendered3D
    // stays true and the 2D path below is never taken.
    bool rendered3D = false;
    if (mCanvas3D != nullptr && mFrame3DInput.IsConnected())
    {
        const Dia::Graphics3D::FrameData3D* frame3D = mFrame3DInput.FetchLatest();
        if (frame3D != nullptr && frame3D->GetMeshDraws().Size() > 0)
        {
            mLastFrame3D.Copy(*frame3D);
            // StartFrame and EndFrame take FrameData& — FrameData3D IS-A FrameData
            mCanvas3D->StartFrame(mLastFrame3D);
            mCanvas3D->ProcessFrame(mLastFrame3D);
            mCanvas3D->EndFrame(mLastFrame3D);
            rendered3D = true;
        }
    }

    if (!rendered3D)
    {
        const Dia::Graphics::FrameData* frame = mFrameInput.FetchLatest();
        if (frame != nullptr)
        {
            // Preserve UI buffer from last frame if new frame has empty UI.
            // Cross-thread timing can cause RenderPU to fetch stale frames from
            // before SimPU wrote the UI-composited frame, causing UI to flash.
            const Dia::UI::UIDataBuffer& newUI = frame->GetUIData();
            int newSize = newUI.GetBufferSize();
            int oldSize = mLastFrame.GetUIData().GetBufferSize();

            // MUST copy old buffer BEFORE overwriting mLastFrame
            Dia::UI::UIDataBuffer preservedUI;
            if (newSize == 0 && oldSize > 0)
            {
                preservedUI = mLastFrame.GetUIData();
            }

            mLastFrame = *frame;

            // Track UI visibility state transitions
            static bool sLastHadUI = false;
            bool hasUI = (newSize > 0 || oldSize > 0);

            if (newSize == 0 && oldSize > 0)
            {
                mLastFrame.RequestDrawUI(preservedUI);
                DIA_LOG_INFO("Rendering", "RenderModule: Preserved UI buffer (%d bytes) - stale frame from cross-thread timing",
                             oldSize);
                if (mMetricUIPreserved)
                    mMetricUIPreserved->Inc();
            }

            bool finalHasUI = (mLastFrame.GetUIData().GetBufferSize() > 0);
            if (finalHasUI != sLastHadUI)
            {
                DIA_LOG_INFO("Rendering", "RenderModule: UI visibility %s (size=%d)",
                             finalHasUI ? "ON" : "OFF",
                             mLastFrame.GetUIData().GetBufferSize());
                sLastHadUI = finalHasUI;
            }
        }
        mCanvas->RenderFrame(mLastFrame);
    }

    Dia::Graphics::RenderFence fence;
    fence.presentedFrame = ++mPresentCount;
    mFenceOutput.Write(fence, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult RenderModule::DoStop()
{
    DIA_LOG_INFO("Application", "RenderModule DoStop entry");

    // Clear metrics
    mMetricUIPreserved = nullptr;

    // Shutdown TextureHandler on the render thread BEFORE bgfx::shutdown —
    // BgfxTextureHandle destructors call bgfx::destroy() which requires a live context.
    if (mTextureHandlerService.IsAvailable())
        mTextureHandlerService.Get().Shutdown();

#ifdef DIA_DEBUG
    // ImGui uses bgfx resources — shut it down before bgfx::shutdown().
    Dia::ImGui::Shutdown();
#endif

    // Call bgfx::shutdown() on the render thread (the thread that called bgfx::init).
    // This clears Canvas::mInitialised so KernelModule::DoStop can safely delete it.
    if (mCanvas != nullptr)
    {
        mCanvas->Shutdown();
        mCanvas = nullptr;
    }

    // Release the cross-PU stop fence. KernelModule::DoStop has been returning
    // kStopping waiting for this — once observed, it tears the window down on
    // MainPU. Must be the very last thing in DoStop, after bgfx is shut down and
    // we no longer touch any window-owned resource.
    KernelModule::SetRenderContextReleased(true);

    DIA_LOG_INFO("Application", "RenderModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void RenderModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mFrameInput.Connect(app);
    mFrame3DInput.Connect(app);
    mFenceOutput.Connect(app);
    mCanvasService.Connect(app);
    mTextureHandlerService.Connect(app);
    mMeshHandlerService.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using RenderModule_ = Cluiche::AppFlow::RenderModule; }
DIA_MODULE(RenderModule_);
DIA_DESCRIBE(RenderModule_::kTypeId, "Submits draw calls each frame using the bgfx canvas; consumes scene and camera FrameStreams.");
