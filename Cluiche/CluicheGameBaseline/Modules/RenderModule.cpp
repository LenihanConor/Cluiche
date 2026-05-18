#include "Modules/RenderModule.h"
#include "Modules/KernelModule.h"

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaSFML/TextureHandler.h>
#include <DiaLogger/DiaLog.h>
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
    // MainPU — their DoStart() calls are concurrent with no cross-PU ordering
    // guarantee. Wait (return kLoading) until KernelModule has created and
    // released the canvas.
    mCanvas = KernelModule::GetStaticCanvas();
    if (mCanvas == nullptr)
    {
        return Dia::ApplicationFlow::StartResult::kLoading;
    }

    DIA_LOG_INFO("Application", "RenderModule DoStart: canvas acquired");

    // Reset the cross-PU stop fence on every (re-)entry. KernelModule::DoStop
    // blocks until this flips back to true in our DoStop.
    KernelModule::SetRenderContextReleased(false);

    // Activate the GL context on this (render) thread.
    // KernelModule deactivates it on the main thread in DoStart so this is safe.
    mCanvas->SetActiveContext(true);

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
        mCanvas->RenderFrame(*frame);
    }
    else
    {
        mEmptyFrame.Clear();
        mCanvas->RenderFrame(mEmptyFrame);
    }

    // Drain GPU resource deletions queued by Unload() on other threads.
    // RenderPU owns the GL context; this is the only place sf::Texture
    // destructors can safely run. See TextureHandler::ProcessGpuDeletions.
    if (Dia::SFML::TextureHandler* tex = KernelModule::GetStaticTextureHandler())
        tex->ProcessGpuDeletions();
}

Dia::ApplicationFlow::StopResult RenderModule::DoStop()
{
    DIA_LOG_INFO("Application", "RenderModule DoStop entry");

    if (mCanvas != nullptr)
    {
        // Final GPU-deletion drain while we still hold the context. Anything
        // Unload()'ed between the last DoUpdate and now (e.g. from a sibling
        // PU's DoStop) must be destroyed before we release the context.
        if (Dia::SFML::TextureHandler* tex = KernelModule::GetStaticTextureHandler())
            tex->ProcessGpuDeletions();

        mCanvas->SetActiveContext(false);
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
}

} } // namespace Cluiche::AppFlow

namespace { using RenderModule_ = Cluiche::AppFlow::RenderModule; }
DIA_MODULE(RenderModule_);
