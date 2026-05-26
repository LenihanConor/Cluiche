#include "Modules/KernelModule.h"

#include <DiaSFML/Window.h>
#include <DiaSFML/InputSource.h>
#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaWindow/Interface/IWindow.h>
#include <DiaInput/EventData.h>
#include <DiaInput/Event.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Histogram.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include "Modules/JobSystemModule.h"

#include <DiaBgfx/Canvas.h>
#include <DiaWindow/SystemHandle.h>

#ifdef DIA_DEBUG
#include <DiaBgfx/Imgui/BgfxImGuiBackend.h>
#include <DiaImGui/DiaImGuiManager.h>
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC  KernelModule::kTypeId("KernelModule");
std::atomic<bool>           KernelModule::sRenderContextReleased{false};
std::atomic<bool>           KernelModule::sRenderContextActive{false};

KernelModule::KernelModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult KernelModule::DoStart()
{
    DIA_LOG_INFO("Application", "KernelModule DoStart entry");

    Dia::Window::IWindow::Settings windowSetting(
        "CluicheTest",
        Dia::Window::IWindow::Settings::Dimensions(1400, 1000),
        Dia::Window::IWindow::Settings::Style());

    mWindow = mWindowFactory.Create(windowSetting);

    Dia::SFML::Window* sfmlWindow = static_cast<Dia::SFML::Window*>(mWindow);

    // Wire the JobSystem into TextureHandler for async asset loading.
    auto* jobSystemModule = Cluiche::AppFlow::JobSystemModule::GetStatic();
    DIA_ASSERT(jobSystemModule != nullptr, "TextureHandler requires JobSystemModule to be initialized first");
    mTextureHandler.SetJobSystem(&jobSystemModule->GetJobSystem());
    mTextureHandlerService.Register(mTextureHandler);

    sfmlWindow->ListenForInputSources(Dia::Core::BitArray8(
        Dia::SFML::InputSource::ESources::kSystem |
        Dia::SFML::InputSource::ESources::kKeyboard |
        Dia::SFML::InputSource::ESources::kMouse));

    mInputSourceManager.AddInputSource(sfmlWindow);
    mInputSourceManager.AddInputSource(&mGamepadManager);

    // Construct bgfx Canvas unconditionally — SFML render path is removed.
    Dia::Bgfx::CanvasSettings bgfxSettings;
    bgfxSettings.initialSize = Dia::Maths::Vector2D(1400.0f, 1000.0f);
    bgfxSettings.cookedShaderRoot = "shaders";
    bgfxSettings.rendererType = Dia::Bgfx::RendererType::Direct3D11;

    mBgfxCanvas = new Dia::Bgfx::Canvas();

    Dia::Window::SystemHandle hwnd = sfmlWindow->GetSystemHandle();
    mBgfxCanvas->AttachToNativeWindow(hwnd, bgfxSettings.initialSize);
    mBgfxCanvas->Initialize(bgfxSettings);

    mCanvas = mBgfxCanvas;

    // Deactivate SFML's implicit GL context — bgfx owns the window surface.
    // RenderModule activates the context on the RenderPU thread.
    mCanvas->SetActiveContext(false);

#ifdef DIA_DEBUG
    mBgfxImGuiBackend = new Dia::Bgfx::BgfxImGuiBackend();
    mBgfxImGuiBackend->Configure(mBgfxCanvas->GetImGuiViewId(), hwnd, mBgfxCanvas);
    Dia::ImGui::SetBackend(mBgfxImGuiBackend);
    Dia::ImGui::Init();
#endif

    // Publish canvas AFTER deactivating the context — RenderModule polls from its thread.
    mCanvasService.Register(*mCanvas);

    {
        auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricInputSources   = reg.RegisterGauge(Dia::Core::StringCRC("dia.input.sources"));
        static const float kEventBuckets[] = { 0.0f, 1.0f, 5.0f, 10.0f, 50.0f, 100.0f };
        mMetricEventsPerFrame = reg.RegisterHistogram(
            Dia::Core::StringCRC("dia.input.events_per_frame"), kEventBuckets, 6);
        mMetricActiveGamepads = reg.RegisterGauge(Dia::Core::StringCRC("dia.input.active_gamepads"));
        mMetricInputSources->Set(2.0);
    }

    DIA_LOG_INFO("Application", "KernelModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void KernelModule::DoUpdate(float /*dt*/)
{
    mFrameEvents.RemoveAll();

    mInputSourceManager.StartFrame();
    mInputSourceManager.Update(mFrameEvents);
    mInputSourceManager.EndFrame();

    if (mMetricEventsPerFrame)
        mMetricEventsPerFrame->Observe(static_cast<double>(mFrameEvents.Size()));
    if (mMetricActiveGamepads)
        mMetricActiveGamepads->Set(static_cast<double>(mGamepadManager.GetActiveGamepadCount()));

    for (unsigned int i = 0; i < mFrameEvents.Size(); ++i)
    {
        const Dia::Input::Event& ev = mFrameEvents[i];

        MainToSimEvent envelope;
        envelope.kind  = MainToSimEvent::Kind::kInput;
        envelope.input = ev;
        (void)mInputWriter.Send(envelope);

        if (ev.type == Dia::Input::Event::EType::kClosed)
        {
            if (auto* app = GetApplication())
                app->RequestShutdown();
        }
    }
}

Dia::ApplicationFlow::StopResult KernelModule::DoStop()
{
    if (!IsRenderContextReleased())
    {
        return Dia::ApplicationFlow::StopResult::kStopping;
    }

    DIA_LOG_INFO("Application", "KernelModule DoStop entry");

#ifdef DIA_DEBUG
    if (mBgfxImGuiBackend != nullptr)
    {
        delete mBgfxImGuiBackend;
        mBgfxImGuiBackend = nullptr;
    }
#endif

    if (mBgfxCanvas != nullptr)
    {
        delete mBgfxCanvas;
        mBgfxCanvas = nullptr;
    }

    mWindowFactory.Destroy(mWindow);
    mWindow  = nullptr;
    mCanvas  = nullptr;

    mMetricInputSources   = nullptr;
    mMetricEventsPerFrame = nullptr;
    mMetricActiveGamepads = nullptr;

    DIA_LOG_INFO("Application", "KernelModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void KernelModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mInputWriter.Connect(app);
    mCanvasService.Connect(app);
    mTextureHandlerService.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using KernelModule_ = Cluiche::AppFlow::KernelModule; }
DIA_MODULE(KernelModule_);
