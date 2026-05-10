#include "Modules/RenderModule.h"
#include "Modules/KernelModule.h"

#include <DiaGraphics/Interface/ICanvas.h>
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
}

Dia::ApplicationFlow::StopResult RenderModule::DoStop()
{
    DIA_LOG_INFO("Application", "RenderModule DoStop entry");

    if (mCanvas != nullptr)
    {
        mCanvas->SetActiveContext(false);
        mCanvas = nullptr;
    }

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
