#include "Modules/LoadingScreenModule.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cmath>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC LoadingScreenModule::kTypeId("LoadingScreenModule");

LoadingScreenModule::LoadingScreenModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
}

Dia::ApplicationFlow::StartResult LoadingScreenModule::DoStart()
{
    DIA_LOG_INFO("Application", "LoadingScreenModule::DoStart entry");
    mElapsed = 0.0f;
    DIA_LOG_INFO("Application", "LoadingScreenModule::DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void LoadingScreenModule::DoUpdate(float dt)
{
    mElapsed += dt;
    mLoadingFrame.Clear();

    // Composite the latest UI buffer so the bootscreen page is visible
    // during the Boot stage.
    if (const Dia::UI::UIDataBuffer* uiBuffer = mUIInput.FetchLatest())
    {
        mLoadingFrame.RequestDrawUI(*uiBuffer);
    }

    // Heartbeat debug draw so the Boot stage has visible sim/render activity
    // even before the user hits Launch.
    const float kRadius = 60.0f;
    Dia::Maths::Vector2D center(100.0f, 100.0f);
    Dia::Maths::Vector2D dynamic(
        center.x + std::cos(mElapsed) * kRadius,
        center.y + std::sin(mElapsed) * kRadius);
    mLoadingFrame.RequestDraw(center,  75.0f, Dia::Graphics::RGBA::White);
    mLoadingFrame.RequestDraw(dynamic, 25.0f, Dia::Graphics::RGBA::Red);
    mLoadingFrame.RequestDraw(center, dynamic, Dia::Graphics::RGBA::White);

    mRenderOutput.Write(mLoadingFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult LoadingScreenModule::DoStop()
{
    DIA_LOG_INFO("Application", "LoadingScreenModule::DoStop entry");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void LoadingScreenModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mUIInput.Connect(app);
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using LoadingScreenModule_ = Cluiche::AppFlow::LoadingScreenModule; }
DIA_MODULE(LoadingScreenModule_);
