#include "Modules/RenderModule.h"
#include "Modules/KernelModule.h"

#include <DiaBgfx/Handlers/TextureHandler.h>
#include <DiaObservation/Log/DiaLog.h>
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

    DIA_LOG_INFO("Application", "RenderModule DoStart: canvas acquired");

    // Reset the cross-PU stop fence on every (re-)entry. KernelModule::DoStop
    // blocks until this flips back to true in our DoStop.
    KernelModule::SetRenderContextReleased(false);

    // Signal to DebugUIModule (same PU) that the render context is ready for ImGui.
    KernelModule::SetRenderContextActive(true);

    // VSync is configured via ICanvas::Settings at initialization time (in KernelModule).
    // No runtime SetVSync API exists on ICanvas; VSyncEnum::kEnable is the default.

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

    const Dia::Graphics::FrameData* frame = mFrameInput.FetchLatest();
    if (frame != nullptr)
    {
        mLastFrame = *frame;
    }

    mCanvas->RenderFrame(mLastFrame);

    Dia::Graphics::RenderFence fence;
    fence.presentedFrame = ++mPresentCount;
    mFenceOutput.Write(fence, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult RenderModule::DoStop()
{
    DIA_LOG_INFO("Application", "RenderModule DoStop entry");

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
    mFenceOutput.Connect(app);
    mCanvasService.Connect(app);
    mTextureHandlerService.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using RenderModule_ = Cluiche::AppFlow::RenderModule; }
DIA_MODULE(RenderModule_);
DIA_DESCRIBE(RenderModule_::kTypeId, "Submits draw calls each frame using the bgfx canvas; consumes scene and camera FrameStreams.");
