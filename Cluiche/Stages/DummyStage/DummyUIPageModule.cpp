#include "DummyUIPageModule.h"

#include <DiaLogger/DiaLog.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche { namespace AppFlow {

DummyUIPageModule::DummyUIPageModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
    , mPage(this)
{}

Dia::ApplicationFlow::StartResult DummyUIPageModule::DoStart()
{
    DIA_LOG_INFO("Application", "DummyUIPageModule DoStart entry");

    UIModule* ui = mUI.Get();
    if (ui == nullptr || !ui->HasStarted())
    {
        return Dia::ApplicationFlow::StartResult::kLoading;
    }

    if (!mLoaded)
    {
        mPage.InitializePage();
        ui->LoadPage(mPage);
        mLoaded = true;
        DIA_LOG_INFO("Application", "DummyUIPageModule loaded dummyStage page");
    }

    DIA_LOG_INFO("Application", "DummyUIPageModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void DummyUIPageModule::DoUpdate(float /*dt*/)
{
    // Page rendering is driven by UIModule; no per-frame work here.
}

Dia::ApplicationFlow::StopResult DummyUIPageModule::DoStop()
{
    DIA_LOG_INFO("Application", "DummyUIPageModule DoStop entry");

    if (mLoaded)
    {
        if (UIModule* ui = mUI.Get())
            ui->UnloadPage();
        mLoaded = false;
    }

    DIA_LOG_INFO("Application", "DummyUIPageModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void DummyUIPageModule::RequestExitLevel()
{
    DIA_LOG_INFO("Application", "DummyUIPageModule: Application_ExitLevel -> TransitionTo('Boot')");
    TransitionTo(Dia::Core::StringCRC("Boot"));
}

const Dia::Core::StringCRC DummyUIPageModule::kTypeId("DummyUIPageModule");

} } // namespace Cluiche::AppFlow

namespace { using DummyUIPageModule_ = Cluiche::AppFlow::DummyUIPageModule; }
DIA_MODULE(DummyUIPageModule_);
