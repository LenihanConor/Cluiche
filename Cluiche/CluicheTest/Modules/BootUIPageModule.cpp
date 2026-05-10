#include "Modules/BootUIPageModule.h"

#include <DiaLogger/DiaLog.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche { namespace AppFlow {

BootUIPageModule::BootUIPageModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
    , mPage(this)
{}

Dia::ApplicationFlow::StartResult BootUIPageModule::DoStart()
{
    DIA_LOG_INFO("Application", "BootUIPageModule DoStart entry");

    UIModule* ui = mUI.Get();
    if (ui == nullptr || !ui->HasStarted())
    {
        // UIModule is on the same PU but may start in a later tick — retry.
        return Dia::ApplicationFlow::StartResult::kLoading;
    }

    if (!mLoaded)
    {
        mPage.InitializePage();
        ui->LoadPage(mPage);
        mLoaded = true;
        DIA_LOG_INFO("Application", "BootUIPageModule loaded bootscreen page");
    }

    DIA_LOG_INFO("Application", "BootUIPageModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void BootUIPageModule::DoUpdate(float /*dt*/)
{
    // Pages are driven by UIModule's Update; nothing per-frame here.
}

Dia::ApplicationFlow::StopResult BootUIPageModule::DoStop()
{
    DIA_LOG_INFO("Application", "BootUIPageModule DoStop entry");

    if (mLoaded)
    {
        if (UIModule* ui = mUI.Get())
            ui->UnloadPage();
        mLoaded = false;
    }

    DIA_LOG_INFO("Application", "BootUIPageModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void BootUIPageModule::RequestLaunchLevel(const Dia::Core::Containers::String64& levelName)
{
    DIA_LOG_INFO("Application", "BootUIPageModule: Application_LaunchLevel('%s') -> TransitionTo('DummyStage')",
        levelName.AsCStr());

    // v2 maps v1's "levelName" to a stage id. Only DummyStage exists today.
    TransitionTo(Dia::Core::StringCRC("DummyStage"));
}

const Dia::Core::StringCRC BootUIPageModule::kTypeId("BootUIPageModule");

} } // namespace Cluiche::AppFlow

namespace { using BootUIPageModule_ = Cluiche::AppFlow::BootUIPageModule; }
DIA_MODULE(BootUIPageModule_);
