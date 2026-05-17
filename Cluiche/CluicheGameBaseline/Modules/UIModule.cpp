#include "Modules/UIModule.h"

#include <DiaLogger/DiaLog.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaUI/IUISystem.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaUIUltralight/UltralightUISystem.h>
#include <DiaInput/Event.h>
#include <DiaInput/EventData.h>

namespace Cluiche { namespace AppFlow {

UIModule::UIModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult UIModule::DoStart()
{
    DIA_LOG_INFO("Application", "UIModule DoStart entry");

    // UIModule depends on KernelModule (same PU), so KernelModule has already
    // started and its window is available by the time this DoStart fires.
    KernelModule* kernel = mKernel.Get();
    DIA_ASSERT(kernel != nullptr, "UIModule::DoStart — KernelModule not found in same PU");
    if (kernel == nullptr || kernel->GetWindow() == nullptr)
    {
        DIA_LOG_ERROR("Application", "UIModule::DoStart — KernelModule window not available");
        return Dia::ApplicationFlow::StartResult::kFailed;
    }

    mUISystem = new Dia::UI::Ultralight::UISystem(kernel->GetWindow());
    mUISystem->Initialize();

    mHasStarted = true;

    DIA_LOG_INFO("Application", "UIModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void UIModule::DoUpdate(float /*dt*/)
{
    if (mUISystem == nullptr)
        return;

    // Inject input from KernelModule's per-frame event buffer (same PU).
    if (KernelModule* kernel = mKernel.Get())
    {
        const Dia::Input::EventData& events = kernel->GetFrameInputEvents();
        for (unsigned int i = 0; i < events.Size(); ++i)
        {
            const Dia::Input::Event& ev = events[i];
            if (ev.type == Dia::Input::Event::EType::kMouseMoved)
            {
                mUISystem->InjectMouseMove(ev.mouseMove.x, ev.mouseMove.y);
            }
            else if (ev.type == Dia::Input::Event::EType::kMouseButtonReleased)
            {
                mUISystem->InjectMouseClick(ev.mouseButton.AsMouseButton(), ev.mouseButton.x, ev.mouseButton.y);
            }
        }
    }

    // Drain HUD commands from Sim (FPS, Score, etc.).
    Dia::Core::Containers::DynamicArrayC<UICommand, 32> pending;
    mUICommands.Consume(pending);

    mUISystem->Update();

    // Publish the fresh UI buffer so the sim/render path can composite it.
    Dia::UI::UIDataBuffer buffer;
    if (mUISystem->IsPageLoaded())
    {
        mUISystem->FetchUIDataBuffer(buffer);
    }
    mUIBufferOutput.Write(buffer, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult UIModule::DoStop()
{
    DIA_LOG_INFO("Application", "UIModule DoStop entry");

    mHasStarted = false;

    if (mUISystem != nullptr)
    {
        if (mUISystem->IsPageLoaded())
            mUISystem->UnloadPage();
        mUISystem->Shutdown();
        delete mUISystem;
        mUISystem = nullptr;
    }

    DIA_LOG_INFO("Application", "UIModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void UIModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mUICommands.Connect(app);
    mUIBufferOutput.Connect(app);
}

void UIModule::LoadPage(Dia::UI::Page& page)
{
    if (mUISystem != nullptr)
        mUISystem->LoadPage(page);
}

void UIModule::UnloadPage()
{
    if (mUISystem != nullptr && mUISystem->IsPageLoaded())
        mUISystem->UnloadPage();
}

const Dia::Core::StringCRC UIModule::kTypeId("UIModule");

} } // namespace Cluiche::AppFlow

namespace { using UIModule_ = Cluiche::AppFlow::UIModule; }
DIA_MODULE(UIModule_);
