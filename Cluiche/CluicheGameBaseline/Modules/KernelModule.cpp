#include "Modules/KernelModule.h"

#include <DiaSFML/RenderWindow.h>
#include <DiaSFML/InputSource.h>
#include <DiaSFML/TextureHandler.h>
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

#include <cstdlib>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC KernelModule::kTypeId("KernelModule");
Dia::Graphics::ICanvas*    KernelModule::sCanvas         = nullptr;
Dia::SFML::TextureHandler* KernelModule::sTextureHandler = nullptr;
std::atomic<bool>          KernelModule::sRenderContextReleased{false};

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

    Dia::Graphics::ICanvas::Settings canvasSettings(
        Dia::Graphics::ICanvas::Settings::VSyncEnum::kEnable,
        0, 0, 2, 0);

    Dia::SFML::RenderWindow* renderWindow = static_cast<Dia::SFML::RenderWindow*>(
        mWindowFactory.Create(windowSetting, canvasSettings));

    mWindow = renderWindow;
    mCanvas = renderWindow;
    sTextureHandler = renderWindow->GetTextureHandler();

    // Wire the JobSystem into TextureHandler for async asset loading
    auto* jobSystemModule = Cluiche::AppFlow::JobSystemModule::GetStatic();
    DIA_ASSERT(jobSystemModule != nullptr, "TextureHandler requires JobSystemModule to be initialized first");
    sTextureHandler->SetJobSystem(&jobSystemModule->GetJobSystem());

    renderWindow->ListenForInputSources(Dia::Core::BitArray8(
        Dia::SFML::InputSource::ESources::kSystem |
        Dia::SFML::InputSource::ESources::kKeyboard |
        Dia::SFML::InputSource::ESources::kMouse));

    mInputSourceManager.AddInputSource(renderWindow);
    mInputSourceManager.AddInputSource(&mGamepadManager);

    mCanvas->SetActiveContext(false);

    // Check BGFX_BACKEND env var — if set, construct Dia::Bgfx::Canvas alongside SFML.
    // The bgfx canvas is the active canvas; SFML canvas is still alive but not driven.
    const char* bgfxBackendEnv = std::getenv("BGFX_BACKEND");
    if (bgfxBackendEnv != nullptr)
    {
        DIA_LOG_INFO("Application", "KernelModule: BGFX_BACKEND=%s — constructing Bgfx::Canvas", bgfxBackendEnv);

        Dia::Bgfx::CanvasSettings bgfxSettings;
        bgfxSettings.initialSize = Dia::Maths::Vector2D(1400.0f, 1000.0f);
        bgfxSettings.cookedShaderRoot = "Cluiche/out/cluichetest/shaders";

        if (strcmp(bgfxBackendEnv, "dx12") == 0)
            bgfxSettings.rendererType = Dia::Bgfx::RendererType::Direct3D12;
        else if (strcmp(bgfxBackendEnv, "vulkan") == 0)
            bgfxSettings.rendererType = Dia::Bgfx::RendererType::Vulkan;
        else
            bgfxSettings.rendererType = Dia::Bgfx::RendererType::Direct3D11;

        mBgfxCanvas = new Dia::Bgfx::Canvas();

        Dia::Window::SystemHandle hwnd = renderWindow->GetSystemHandle();
        mBgfxCanvas->AttachToNativeWindow(hwnd, bgfxSettings.initialSize);
        mBgfxCanvas->Initialize(bgfxSettings);

        Dia::SFML::TextureHandler::SetBgfxActive(true);
        mCanvas = mBgfxCanvas;
    }

    // Publish sCanvas AFTER deactivating the GL context. RenderModule polls this
    // pointer from its dedicated thread — it must not see a non-null canvas while
    // the context is still active on MainPU.
    sCanvas = mCanvas;

    // Register input metrics with the global MetricRegistry.
    {
        auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricInputSources   = reg.RegisterGauge(Dia::Core::StringCRC("dia.input.sources"));
        static const float kEventBuckets[] = { 0.0f, 1.0f, 5.0f, 10.0f, 50.0f, 100.0f };
        mMetricEventsPerFrame = reg.RegisterHistogram(
            Dia::Core::StringCRC("dia.input.events_per_frame"), kEventBuckets, 6);
        mMetricActiveGamepads = reg.RegisterGauge(Dia::Core::StringCRC("dia.input.active_gamepads"));
        // 2 sources added above (renderWindow + gamepadManager)
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

        (void)mInputWriter.Send(ev);

        if (ev.type == Dia::Input::Event::EType::kClosed)
        {
            if (auto* app = GetApplication())
                app->RequestShutdown();
        }
    }
}

Dia::ApplicationFlow::StopResult KernelModule::DoStop()
{
    // Wait for RenderModule (RenderPU) to release the GL context before we
    // destroy the window. Both modules flip to kStopping in the same
    // BeginStopAllActive() pass and tick concurrently on different PU threads;
    // without this fence ~RenderWindow can race RenderPU's final GL calls and
    // hang inside the GL driver. RenderModule sets this flag true after its
    // SetActiveContext(false).
    if (!IsRenderContextReleased())
    {
        return Dia::ApplicationFlow::StopResult::kStopping;
    }

    DIA_LOG_INFO("Application", "KernelModule DoStop entry");

    sCanvas         = nullptr;
    sTextureHandler = nullptr;

    if (mBgfxCanvas != nullptr)
    {
        delete mBgfxCanvas;
        mBgfxCanvas = nullptr;
    }

    mWindowFactory.Destroy(mWindow);
    mWindow  = nullptr;
    mCanvas  = nullptr;

    // Null metric pointers — MetricRegistry owns the objects.
    mMetricInputSources   = nullptr;
    mMetricEventsPerFrame = nullptr;
    mMetricActiveGamepads = nullptr;

    DIA_LOG_INFO("Application", "KernelModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void KernelModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mInputWriter.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using KernelModule_ = Cluiche::AppFlow::KernelModule; }
DIA_MODULE(KernelModule_);
