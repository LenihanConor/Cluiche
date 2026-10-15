#include "Modules/BusRecorderModule.h"

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {

const Dia::Core::StringCRC BusRecorderModule::kTypeId("BusRecorderModule");

BusRecorderModule::BusRecorderModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

#ifdef DIA_DEBUG

uint64_t BusRecorderModule::ReadTick(void* context)
{
    const BusRecorderModule* self = static_cast<const BusRecorderModule*>(context);
    return self->GetProcessingUnit()->GetSimTimeContext().tick;
}

Dia::ApplicationFlow::StartResult BusRecorderModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;

    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (!host)
        return StartResult::kLoading;

    Dia::MessageBus::FileBusRecorder::Config config;
    config.tickSource  = &BusRecorderModule::ReadTick;
    config.tickContext = this;
    if (!mRecorder.Open(kRecordPath, config))
    {
        // The recorder has already logged why. Recording is optional, so the app runs on.
        return StartResult::kReady;
    }

    host->GetBus().SetRecorder(&mRecorder);
    mAttached = true;
    DIA_LOG_INFO("CoW", "BusRecorderModule: recording bus traffic to %s", kRecordPath);
    return StartResult::kReady;
}

void BusRecorderModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    if (mAttached && ctx.tick % kFlushIntervalTicks == 0)
        mRecorder.Flush();
}

Dia::ApplicationFlow::StopResult BusRecorderModule::DoStop()
{
    if (mAttached)
    {
        if (Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get())
            host->GetBus().SetRecorder(nullptr);
        mAttached = false;
    }
    mRecorder.Close();
    return Dia::ApplicationFlow::StopResult::kDone;
}

#else

Dia::ApplicationFlow::StartResult BusRecorderModule::DoStart() { return Dia::ApplicationFlow::StartResult::kReady; }
void BusRecorderModule::DoUpdate(const Dia::SimTime::SimTimeContext&) {}
Dia::ApplicationFlow::StopResult BusRecorderModule::DoStop() { return Dia::ApplicationFlow::StopResult::kDone; }

#endif

} // namespace CoW

namespace { using BusRecorderModule_ = CoW::BusRecorderModule; }
DIA_MODULE(BusRecorderModule_);
DIA_DESCRIBE(BusRecorderModule_::kTypeId, "Records bus traffic to a file in Debug builds.");
