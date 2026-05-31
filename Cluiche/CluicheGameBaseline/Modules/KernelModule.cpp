#include "Modules/KernelModule.h"

#include <DiaSDL/InputSource.h>
#include <DiaInput/IInputSource.h>
#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaWindow/Interface/IWindow.h>
#include <DiaInput/EventData.h>
#include <DiaInput/Event.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Session/SessionManager.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Histogram.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

#include <DiaBgfx/Canvas.h>
#include <DiaWindow/SystemHandle.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaSDL/DisplayInfo.h>

#ifdef DIA_DEBUG
#include <DiaBgfx/Imgui/BgfxImGuiBackend.h>
#include <DiaImGui/DiaImGuiManager.h>
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC  KernelModule::kTypeId("KernelModule");
std::atomic<bool>           KernelModule::sRenderContextReleased{false};
std::atomic<bool>           KernelModule::sRenderContextActive{false};
std::atomic<unsigned int>   KernelModule::sWindowWidth{1400};
std::atomic<unsigned int>   KernelModule::sWindowHeight{1000};

KernelModule::KernelModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

void KernelModule::OnConfigure(const char* /*configJson*/)
{
    // Window config is read from the .diagame config block in DoStart.
}

static unsigned int ResolveWindowDimension(const Json::Value& win,
                                            const char* absKey, const char* pctKey,
                                            unsigned int fallback,
                                            unsigned int screenSize)
{
    if (win.isMember(absKey) && win[absKey].isInt())
        return static_cast<unsigned int>(win[absKey].asInt());
    if (win.isMember(pctKey) && win[pctKey].isInt())
    {
        const int pct = win[pctKey].asInt();
        if (pct > 0 && pct <= 100)
            return static_cast<unsigned int>(screenSize * pct / 100);
    }
    return fallback;
}

Dia::ApplicationFlow::StartResult KernelModule::DoStart()
{
    DIA_LOG_INFO("Application", "KernelModule DoStart entry");

    // Read window settings from .diagame config
    const Json::Value* cfg = GetApplication()->GetDiagameConfig();
    DIA_LOG_INFO("Application", "KernelModule: diagameConfig %s", cfg ? "found" : "NULL — using hardcoded defaults");

    if (cfg)
    {
        const bool hasWindow = cfg->isMember("window") && (*cfg)["window"].isObject();
        DIA_LOG_INFO("Application", "KernelModule: window block %s", hasWindow ? "found" : "missing");

        if (hasWindow)
        {
            const Json::Value& win = (*cfg)["window"];

            unsigned int screenW = 1920, screenH = 1080;
            Dia::SDL::GetPrimaryDisplaySize(screenW, screenH);
            DIA_LOG_INFO("Application", "KernelModule: screen size %ux%u", screenW, screenH);

            mWindowWidth  = ResolveWindowDimension(win, "width",  "width_pct",  mWindowWidth,  screenW);
            mWindowHeight = ResolveWindowDimension(win, "height", "height_pct", mWindowHeight, screenH);

            if (win.isMember("title") && win["title"].isString())
            {
                mWindowTitle = Dia::Core::Containers::String64(win["title"].asCString());
                DIA_LOG_INFO("Application", "KernelModule: title from diagame = '%s'", mWindowTitle.AsCStr());
            }
        }
    }

    sWindowWidth.store(mWindowWidth, std::memory_order_release);
    sWindowHeight.store(mWindowHeight, std::memory_order_release);

    DIA_LOG_INFO("Application", "KernelModule: opening window '%s' %ux%u",
        mWindowTitle.AsCStr(), mWindowWidth, mWindowHeight);

    Dia::Window::IWindow::Settings windowSetting(
        mWindowTitle,
        Dia::Window::IWindow::Settings::Dimensions(mWindowWidth, mWindowHeight),
        Dia::Window::IWindow::Settings::Style());

    mWindow = mWindowFactory.Create(windowSetting);

    // Wire the JobSystem into TextureHandler for async asset loading.
    DIA_ASSERT(mJobSystemRef.Get() != nullptr, "TextureHandler requires JobSystemModule to be initialized first");
    mTextureHandler.SetJobSystem(&mJobSystemRef->GetJobSystem());
    mTextureHandlerService.Register(mTextureHandler);

    {
        Dia::Core::BitArray8 inputMask;
        inputMask.SetBit(Dia::SDL::InputSource::ESourceIndex::kSystem,   true);
        inputMask.SetBit(Dia::SDL::InputSource::ESourceIndex::kKeyboard, true);
        inputMask.SetBit(Dia::SDL::InputSource::ESourceIndex::kMouse,    true);
        static_cast<Dia::SDL::Window*>(mWindow)->ListenForInputSources(inputMask);
    }

    mInputSourceManager.AddInputSource(static_cast<Dia::SDL::Window*>(mWindow));
    mInputSourceManager.AddInputSource(&mGamepadManager);

    // Construct bgfx Canvas unconditionally — SFML render path is removed.
    Dia::Bgfx::CanvasSettings bgfxSettings;
    bgfxSettings.initialSize = Dia::Maths::Vector2D(
        static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight));
    bgfxSettings.cookedShaderRoot = "shaders";
    bgfxSettings.rendererType = Dia::Bgfx::RendererType::Direct3D11;

    mBgfxCanvas = new Dia::Bgfx::Canvas();

    Dia::Window::SystemHandle hwnd = mWindow->GetSystemHandle();
    mBgfxCanvas->AttachToNativeWindow(hwnd, bgfxSettings.initialSize);
    mBgfxCanvas->Initialize(bgfxSettings);

    mCanvas = mBgfxCanvas;
    Dia::Observation::SessionManager::SetActiveCaptureCanvas(mCanvas);

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
