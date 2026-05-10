#include "Modules/KernelModule.h"

#include <DiaSFML/RenderWindow.h>
#include <DiaSFML/InputSource.h>
#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaWindow/Interface/IWindow.h>
#include <DiaInput/EventData.h>
#include <DiaInput/Event.h>
#include <DiaLogger/DiaLog.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC KernelModule::kTypeId("KernelModule");
Dia::Graphics::ICanvas*    KernelModule::sCanvas         = nullptr;
Dia::SFML::TextureHandler* KernelModule::sTextureHandler = nullptr;

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
    sCanvas = renderWindow;
    sTextureHandler = renderWindow->GetTextureHandler();

    renderWindow->ListenForInputSources(Dia::Core::BitArray8(
        Dia::SFML::InputSource::ESources::kSystem |
        Dia::SFML::InputSource::ESources::kKeyboard |
        Dia::SFML::InputSource::ESources::kMouse));

    mInputSourceManager.AddInputSource(renderWindow);
    mInputSourceManager.AddInputSource(&mGamepadManager);

    mCanvas->SetActiveContext(false);

    DIA_LOG_INFO("Application", "KernelModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void KernelModule::DoUpdate(float /*dt*/)
{
    mFrameEvents.RemoveAll();

    mInputSourceManager.StartFrame();
    mInputSourceManager.Update(mFrameEvents);
    mInputSourceManager.EndFrame();

    for (unsigned int i = 0; i < mFrameEvents.Size(); ++i)
    {
        const Dia::Input::Event& ev = mFrameEvents[i];

        mInputWriter.Send(ev);

        if (ev.type == Dia::Input::Event::EType::kClosed)
        {
            if (auto* app = GetApplication())
                app->RequestShutdown();
        }
    }
}

Dia::ApplicationFlow::StopResult KernelModule::DoStop()
{
    DIA_LOG_INFO("Application", "KernelModule DoStop entry");

    sCanvas         = nullptr;
    sTextureHandler = nullptr;
    mWindowFactory.Destroy(mWindow);
    mWindow  = nullptr;
    mCanvas  = nullptr;

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
