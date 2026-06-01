#include "Modules/UICompositeModule.h"
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Time/TimeAbsolute.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC UICompositeModule::kTypeId("UICompositeModule");

UICompositeModule::UICompositeModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult UICompositeModule::DoStart()
{
    return Dia::ApplicationFlow::StartResult::kReady;
}

void UICompositeModule::DoUpdate(float /*dt*/)
{
    mFrame.Clear();
    if (const Dia::UI::UIDataBuffer* uiBuffer = mUIInput.FetchLatest())
        mFrame.RequestDrawUI(*uiBuffer);
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult UICompositeModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

void UICompositeModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mUIInput.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using UICompositeModule_ = Cluiche::AppFlow::UICompositeModule; }
DIA_MODULE(UICompositeModule_);
