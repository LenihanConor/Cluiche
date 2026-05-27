#include "Modules/RenderModule.h"
#include "Modules/KernelModule.h"

#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

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

    const Dia::Graphics::FrameData* frame = mFrameInput.FetchLatest();
    if (frame != nullptr)
    {
        mLastFrame = *frame;
    }

    mCanvas->RenderFrame(mLastFrame);
}

Dia::ApplicationFlow::StopResult RenderModule::DoStop()
{
    DIA_LOG_INFO("Application", "RenderModule DoStop entry");

    // Shutdown TextureHandler on the render thread BEFORE bgfx::shutdown —
    // BgfxTextureHandle destructors call bgfx::destroy() which requires a live context.
    if (mTextureHandlerService.IsAvailable())
        mTextureHandlerService.Get().Shutdown();

    if (mCanvas != nullptr)
    {
        mCanvas = nullptr;
    }

    // Release the cross-PU stop fence. KernelModule::DoStop has been returning
    // kStopping waiting for this — once observed, it tears the window down on
    // MainPU. Must be the very last thing in DoStop, after the GL context is
    // released and we no longer touch any window-owned resource.
    KernelModule::SetRenderContextReleased(true);

    DIA_LOG_INFO("Application", "RenderModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void RenderModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mFrameInput.Connect(app);
    mCanvasService.Connect(app);
    mTextureHandlerService.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using RenderModule_ = Cluiche::AppFlow::RenderModule; }
DIA_MODULE(RenderModule_);
