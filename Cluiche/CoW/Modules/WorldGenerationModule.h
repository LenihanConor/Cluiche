#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include "Modules/WorldGenerationMessages.h"
#include "Modules/MessageBusHostModule.h"
#include "Modules/RandomStreamModule.h"
#include "Modules/SimQueryHostModule.h"
#include <cstdint>
#include <string>

namespace CoW {

class WorldGenerationHealthReporter : public Dia::Observation::Health::HealthReporterBase
{
public:
    WorldGenerationHealthReporter() { SetOK(); }
    Dia::Core::StringCRC GetReporterName() const override { return Dia::Core::StringCRC("WorldGenerationModule"); }
};

// S-1 World Generation SimPU module: owns a pure WorldGenSession, ticks it each sim frame with the
// WLD random stream and publishes WorldGenerationComplete when a new world revision exists.
// Everything here runs on the SimPU thread only. Commands (Task 4) call GetSession() then NotifyAction().
class WorldGenerationModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "S-1 terrain world generation session, regenerates on demand";

    explicit WorldGenerationModule(const Dia::Core::StringCRC& instanceId);
    ~WorldGenerationModule() override;

    // SimPU thread only. Mutating calls must be followed by NotifyAction().
    WorldGeneration::WorldGenSession& GetSession() { return mSession; }
    const WorldGeneration::WorldGenSession& GetSession() const { return mSession; }

    // nullptr until the first world has been generated.
    const WorldGeneration::WorldStructure* GetCurrentWorld() const
    {
        return mSession.Revision() > 0 ? &mSession.World() : nullptr;
    }
    uint32_t GetWorldRevision() const { return mSession.Revision(); }

    // Lets a module configured with auto_generate=false start generating after an explicit action.
    void NotifyAction() { mActionSeen = true; }

    const WorldGenerationHealthReporter& GetHealthReporter() const { return mHealth; }
    bool IsHealthRegistered() const { return mHealthRegistered; }

    // True once the cow.worldgen.* terrain-slice commands are registered with the SimQueryHostModule
    // (false when no SimQueryHostModule is wired, or before DoStart / after DoStop).
    bool CommandsRegistered() const { return mCommandsRegistered; }

    // Every command and query this module registers, with its full dotted name.
    static constexpr const char* kCommandNames[] = {
        "cow.worldgen.set", "cow.worldgen.set_size", "cow.worldgen.regenerate",
        "cow.worldgen.preset.load", "cow.worldgen.preset.save",
        "cow.worldgen.params", "cow.worldgen.status", "cow.worldgen.presets",
        "cow.worldgen.layers", "cow.worldgen.cell" };
    static constexpr unsigned kCommandCount = sizeof(kCommandNames) / sizeof(kCommandNames[0]);

protected:
    void OnConfigure(const char* configJson) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void RegisterCommands();
    void UnregisterCommands();

    WorldGeneration::WorldGenSession mSession;
    std::string mPresetsDir = "Cluiche/Assets/CoW/Global/WorldGen/Presets";
    std::string mOutPresetsDir = "Cluiche/out/CoW/presets";
    bool mAutoGenerate = true;
    bool mActionSeen = false;
    bool mPublishFailed = false;

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBus{this};
    Dia::ApplicationFlow::ModuleRef<RandomStreamModule> mRandom{this};
    // Optional: the cow.worldgen.* commands are sim-hosted, so they exist only when this module is wired.
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::SimQueryHostModule> mSimQueryHost{this};

    bool mCommandsRegistered = false;

    WorldGenerationHealthReporter mHealth;
    bool mHealthRegistered = false;

    Dia::Observation::Metric::Counter* mGenerationsCounter = nullptr;
    Dia::Observation::Metric::Counter* mPublishDroppedCounter = nullptr;
    Dia::Observation::Metric::Gauge*   mLastMsGauge = nullptr;
    Dia::Observation::Metric::Gauge*   mRevisionGauge = nullptr;
};

} // namespace CoW
