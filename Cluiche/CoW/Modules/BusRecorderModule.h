#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/MessageBusHostModule.h"
#ifdef DIA_DEBUG
#include <DiaMessageBus/FileBusRecorder.h>
#endif
#include <cstdint>

namespace CoW {

// Debug-only: records every dispatched bus message to a file (F-1 Test 6,
// "recorded traffic survives the run"). The recorder mechanism lives in
// DiaMessageBus; this module owns the policy: output path, tick source
// (the SimPU tick) and flush/close lifecycle. Does nothing in Release.
class BusRecorderModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Records bus traffic to a file in Debug builds";

    // Relative to the working directory.
    static constexpr const char* kRecordPath = "cow_bus_traffic.dbtr";
    static constexpr uint64_t kFlushIntervalTicks = 30;

    explicit BusRecorderModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
#ifdef DIA_DEBUG
    static uint64_t ReadTick(void* context);

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBus{this};
    Dia::MessageBus::FileBusRecorder mRecorder;
    bool mAttached = false;
#endif
};

} // namespace CoW
