#include "Modules/StageStatusModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC StageStatusModule::kTypeId("StageStatusModule");

StageStatusModule::StageStatusModule(const Dia::Core::StringCRC& instanceId)
    : RenderModule(instanceId)
{}

bool StageStatusModule::Set(const Dia::Core::StringCRC& stage, const StageStatus& status)
{
    for (int i = 0; i < mCount; ++i)
    {
        if (mEntries[i].stage == stage)
        {
            mEntries[i].status = status;
            return true;
        }
    }
    if (mCount >= kMaxStages)
        return false;
    mEntries[mCount].stage = stage;
    mEntries[mCount].status = status;
    ++mCount;
    return true;
}

const StageStatusModule::StageStatus* StageStatusModule::Get(const Dia::Core::StringCRC& stage) const
{
    for (int i = 0; i < mCount; ++i)
    {
        if (mEntries[i].stage == stage)
            return &mEntries[i].status;
    }
    return nullptr;
}

Dia::ApplicationFlow::StartResult StageStatusModule::DoStart() { return Dia::ApplicationFlow::StartResult::kReady; }
void StageStatusModule::DoUpdate(const Dia::SimTime::RenderTimeContext&) {}
Dia::ApplicationFlow::StopResult StageStatusModule::DoStop() { return Dia::ApplicationFlow::StopResult::kDone; }

} } // namespace Cluiche::AppFlow

namespace { using StageStatusModule_ = Cluiche::AppFlow::StageStatusModule; }
DIA_MODULE(StageStatusModule_);
DIA_DESCRIBE(StageStatusModule_::kTypeId, "Holds per-stage status (label, severity, visited) keyed by stage CRC.");
