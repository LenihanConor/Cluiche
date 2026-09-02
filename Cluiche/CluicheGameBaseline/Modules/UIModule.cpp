#include "Modules/UIModule.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/Event.h>
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
#include <DiaInput/EKeyModifiers.h>

namespace Cluiche { namespace AppFlow {

UIModule::UIModule(const Dia::Core::StringCRC& instanceId)
    : MainModule(instanceId)
{}

UIModule::~UIModule()
{
    delete mUISystem;
    mUISystem = nullptr;
}

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

    // Ultralight/WebCore has process-lifetime globals that cannot be torn down
    // and re-initialized within a process. Allocate once and reuse across stage
    // transitions; only the destructor frees it.
    if (mUISystem == nullptr)
    {
        mUISystem = new Dia::UI::Ultralight::UISystem(kernel->GetWindow());
        mUISystem->Initialize();
    }

    mUISystem->SetInputRouter(&mInputRouter);
    DIA_LOG_INFO("UI", "UIModule: InputRouter wired to UISystem");

    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricKeysInjected)
        mMetricKeysInjected = reg.RegisterGauge(Dia::Core::StringCRC("ui.keys_injected_per_frame"));

    mHasStarted = true;

    DIA_LOG_INFO("Application", "UIModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void UIModule::DoUpdate(const Dia::SimTime::MainTimeContext& /*ctx*/)
{
    DIA_TRACE_ZONE("UIModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    if (mUISystem == nullptr)
        return;

    // Inject input from KernelModule's per-frame event buffer (same PU).
    if (KernelModule* kernel = mKernel.Get())
    {
        const Dia::Input::EventData& events = kernel->GetFrameInputEvents();
        const Dia::Input::EInputRouting routing = mInputRouter.GetCurrentInputMode();
        static const char* kModeNames[] = { "kGameOnly", "kUIOnly", "kGameAndUI" };

        if (routing != mLastRoutingMode)
        {
            const int prev = static_cast<int>(mLastRoutingMode);
            const int curr = static_cast<int>(routing);
            DIA_LOG_INFO("UI", "UIModule: input routing mode changed: %s → %s",
                kModeNames[prev], kModeNames[curr]);
            mLastRoutingMode = routing;
        }

        const bool injectKeyToUI = (routing != Dia::Input::EInputRouting::kGameOnly);
        int keysInjectedThisFrame = 0;

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
            else if (injectKeyToUI)
            {
                if (ev.type == Dia::Input::Event::EType::kKeyPressed)
                {
                    int mods = 0;
                    if (ev.key.shift)   mods |= Dia::Input::kModShift;
                    if (ev.key.control) mods |= Dia::Input::kModControl;
                    if (ev.key.alt)     mods |= Dia::Input::kModAlt;
                    if (ev.key.system)  mods |= Dia::Input::kModSystem;
                    mUISystem->InjectKeyDown(ev.key.AsKey(), mods);
                    ++keysInjectedThisFrame;
                }
                else if (ev.type == Dia::Input::Event::EType::kKeyReleased)
                {
                    int mods = 0;
                    if (ev.key.shift)   mods |= Dia::Input::kModShift;
                    if (ev.key.control) mods |= Dia::Input::kModControl;
                    if (ev.key.alt)     mods |= Dia::Input::kModAlt;
                    if (ev.key.system)  mods |= Dia::Input::kModSystem;
                    mUISystem->InjectKeyUp(ev.key.AsKey(), mods);
                    ++keysInjectedThisFrame;
                }
                else if (ev.type == Dia::Input::Event::EType::kTextEntered)
                {
                    mUISystem->InjectCharacterInput(ev.text.unicode);
                    DIA_LOG_INFO("UI", "UIModule: InjectCharacterInput U+%04X (routing=%s)", ev.text.unicode, kModeNames[static_cast<int>(routing)]);
                    ++keysInjectedThisFrame;
                }
            }
        }

        if (mMetricKeysInjected)
            mMetricKeysInjected->Set(static_cast<double>(keysInjectedThisFrame));
    }

    // Drain HUD commands from Sim (FPS, Score, etc.).
    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<SimToMainEvent>, 32> pending;
    mUICommands.Consume(pending);

    mUISystem->Update();

    // Publish the fresh UI buffer so the sim/render path can composite it.
    Dia::UI::UIDataBuffer buffer;
    const bool pageLoaded = mUISystem->IsPageLoaded();
    if (pageLoaded)
    {
        mUISystem->FetchUIDataBuffer(buffer);
    }
    // Always write the buffer (empty or not) - RenderModule needs consistent empty
    // frames to clear preserved UI, not just one empty frame
    mUIBufferOutput.Write(buffer, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult UIModule::DoStop()
{
    DIA_LOG_INFO("Application", "UIModule DoStop entry");

    mHasStarted = false;
    mMetricKeysInjected = nullptr;

    if (mUISystem != nullptr)
    {
        if (mUISystem->IsPageLoaded())
            mUISystem->UnloadPage();
        mUISystem->Shutdown();
        // Do NOT delete mUISystem here — Ultralight/WebCore process-lifetime
        // globals crash on re-initialization. It is freed in the destructor.
    }

    // Flush an empty buffer so downstream consumers (RenderModule) don't keep
    // compositing stale pixels from the last active stage.
    Dia::UI::UIDataBuffer emptyBuffer;
    mUIBufferOutput.Write(emptyBuffer, Dia::Core::TimeAbsolute::Zero());

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
DIA_DESCRIBE(UIModule_::kTypeId, "Manages the Ultralight-based UI system: page lifecycle, input routing, and JavaScript bridge.");
