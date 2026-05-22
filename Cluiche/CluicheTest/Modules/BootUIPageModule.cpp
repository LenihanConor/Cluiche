#include "Modules/BootUIPageModule.h"
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationControl.h>
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
        CacheNavigableStages();
        mPage.InitializePage();
        ui->LoadPage(mPage);
        mLoaded = true;
        DIA_LOG_INFO("Application", "BootUIPageModule loaded bootscreen page (%u stages)", mNavigableStages.Size());
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
    DIA_LOG_INFO("Application", "BootUIPageModule: Application_LaunchLevel('%s') -> TransitionTo('%s')",
        levelName.AsCStr(), levelName.AsCStr());

    TransitionTo(Dia::Core::StringCRC(levelName.AsCStr()));
}

void BootUIPageModule::CacheNavigableStages()
{
    mNavigableStages.RemoveAll();
    GetApplication()->GetStageTransitions(Dia::Core::StringCRC("Boot"), mNavigableStages);
}

int BootUIPageModule::GetNavigableStageCount()
{
    return static_cast<int>(mNavigableStages.Size());
}

Dia::Core::Containers::String64 BootUIPageModule::GetNavigableStageName(int index)
{
    if (index >= 0 && index < static_cast<int>(mNavigableStages.Size()))
        return Dia::Core::Containers::String64(mNavigableStages[index].AsChar());
    return Dia::Core::Containers::String64("");
}

Dia::Core::Containers::String64 BootUIPageModule::GetStageStatus(int index)
{
    if (index < 0 || index >= static_cast<int>(mNavigableStages.Size()))
        return Dia::Core::Containers::String64("-");

    if (!CluicheTest::TestResultsRegistry::IsCreated())
        return Dia::Core::Containers::String64("-");

    const CluicheTest::StageResult* result =
        CluicheTest::TestResultsRegistry::GetInstance().GetResult(mNavigableStages[index]);
    if (!result)
        return Dia::Core::Containers::String64("-");

    switch (result->state)
    {
    case CluicheTest::StageResult::State::kPassed:  return Dia::Core::Containers::String64("passed");
    case CluicheTest::StageResult::State::kFailed:  return Dia::Core::Containers::String64("failed");
    case CluicheTest::StageResult::State::kTimeout: return Dia::Core::Containers::String64("timeout");
    case CluicheTest::StageResult::State::kRunning: return Dia::Core::Containers::String64("running");
    default:                                        return Dia::Core::Containers::String64("-");
    }
}

const Dia::Core::StringCRC BootUIPageModule::kTypeId("BootUIPageModule");

} } // namespace Cluiche::AppFlow

namespace { using BootUIPageModule_ = Cluiche::AppFlow::BootUIPageModule; }
DIA_MODULE(BootUIPageModule_);
