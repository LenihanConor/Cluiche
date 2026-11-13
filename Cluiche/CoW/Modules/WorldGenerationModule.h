#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
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
    // True once a world of at least this revision exists (the hand-off contract for dependants, SD-S1-010).
    bool IsReady(uint32_t revision) const { return GetWorldRevision() >= revision; }

    // World hand-off (SD-S1-010): empty when the module config named no world or named it correctly. Otherwise the
    // reason the named world could not be loaded (the module stays started, health is Failing, no world is generated
    // until an explicit action).
    const std::string& GetHandoffError() const { return mHandoffError; }

    // Site placers for the regions-and-sites pass. A module that depends on this one registers its kinds here in its
    // own DoStart (and Unregisters them in DoStop). SimPU thread only.
    // Deferral: generation is held back until the first DoUpdate after this module's DoStart (so every dependant that
    // becomes ready in the same start pass has registered), and for as long as any HoldGeneration() is outstanding.
    // A dependant whose DoStart returns kLoading across frames calls HoldGeneration() in its first DoStart call and
    // ReleaseGeneration() once its registrations are in place.
    WorldGeneration::SiteRegistry& GetSiteRegistry() { return mSiteRegistry; }
    const WorldGeneration::SiteRegistry& GetSiteRegistry() const { return mSiteRegistry; }
    void HoldGeneration() { ++mGenerationHolds; }
    void ReleaseGeneration() { if (mGenerationHolds > 0) --mGenerationHolds; }

    // Lets a module configured with auto_generate=false start generating after an explicit action.
    void NotifyAction() { mActionSeen = true; }

    // World file actions: the ONE path behind cow.worldgen.export / cow.worldgen.import and the panel's export / import
    // commands (SD-S1-002). Import also does the module-side follow-up (gauges, health, WorldGenerationComplete).
    // SimPU thread only; the caller still calls NotifyAction().
    Json::Value ExportWorldFile(const Json::Value& args);
    Json::Value ImportWorldFile(const Json::Value& args);

    // Report of the last finished Regenerate (retry attempts, outcome, evidence); nullptr before the first. SimPU only.
    const WorldGeneration::GenerationReport* GetLastReport() const { return mSession.LastReport(); }
    uint32_t GetReportRevision() const { return mSession.ReportRevision(); }

    const WorldGenerationHealthReporter& GetHealthReporter() const { return mHealth; }
    bool IsHealthRegistered() const { return mHealthRegistered; }

    // True once the cow.worldgen.* terrain-slice commands are registered with the SimQueryHostModule
    // (false when no SimQueryHostModule is wired, or before DoStart / after DoStop).
    bool CommandsRegistered() const { return mCommandsRegistered; }

    // Default writable worlds directory (PD-009: generated output lives under Cluiche/out/<AppName>/).
    static constexpr const char* kDefaultOutWorldsDir = "Cluiche/out/CoW/worlds";
    const std::string& GetOutWorldsDirectory() const { return mOutWorldsDir; }

    // Every command and query this module registers, with its full dotted name.
    static constexpr const char* kCommandNames[] = {
        "cow.worldgen.set", "cow.worldgen.set_size", "cow.worldgen.regenerate",
        "cow.worldgen.preset.load", "cow.worldgen.preset.save",
        "cow.worldgen.params", "cow.worldgen.status", "cow.worldgen.presets",
        "cow.worldgen.layers", "cow.worldgen.cell", "cow.worldgen.regions", "cow.worldgen.sites",
        "cow.worldgen.report", "cow.worldgen.export", "cow.worldgen.import", "cow.worldgen.cancel",
        "cow.worldgen.validate", "cow.worldgen.worlds" };
    static constexpr unsigned kCommandCount = sizeof(kCommandNames) / sizeof(kCommandNames[0]);

protected:
    void OnConfigure(const char* configJson) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void RegisterCommands();
    void UnregisterCommands();
    // World hand-off failure: Error log, Failing health, automatic generation suppressed (mHandoffError holds the reason).
    void ReportHandoffFailure();
    // World hand-off, world_file form: imports the named file once (all-or-nothing, opens locked) and announces it.
    void ApplyHandoffWorldFile();
    // A Regenerate finished (any outcome): retry events, the failure event, counters, health. A cancel is not a health failure.
    void OnRetryRunFinished();
    // A world was imported from a file: gauges, health and the WorldGenerationComplete notification (the world changed).
    void OnWorldImported();
    // Logs the outcome of an export or import command reply (Info on success with each warning, Warning on failure).
    void LogWorldFileReply(const char* action, const Json::Value& reply) const;

    WorldGeneration::WorldGenSession mSession;
    WorldGeneration::SiteRegistry mSiteRegistry;
    int  mGenerationHolds = 0;
    bool mStartDeferral = true;      // skip the first DoUpdate: dependants started in the same pass have registered by then
    std::string mPresetsDir = "Cluiche/Assets/CoW/Global/WorldGen/Presets";
    std::string mOutPresetsDir = "Cluiche/out/CoW/presets";
    std::string mWorldsDir;                                        // read-only deployed worlds (none by default)
    std::string mOutWorldsDir = kDefaultOutWorldsDir;              // PD-009: exports land here
    bool mAutoGenerate = true;
    bool mActionSeen = false;
    // World hand-off, read from the module config (see OnConfigure): at most one source.
    std::string mHandoffPreset;       // "preset": applied at the end of OnConfigure
    std::string mHandoffWorldFile;    // "world_file": imported in DoStart
    std::string mHandoffError;        // why the hand-off failed; empty if it did not
    bool mHandoffFailed = false;      // automatic generation is suppressed until an explicit action
    bool mPublishFailed = false;
    uint32_t mPublishedReportRevision = 0;   // last report revision this module has handled
    bool mGenerationUnsatisfiable = false;   // the last Regenerate ended in a failure that is not a cancel (unsatisfiable, timeout, error)

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBus{this};
    Dia::ApplicationFlow::ModuleRef<RandomStreamModule> mRandom{this};
    // Optional: the cow.worldgen.* commands are sim-hosted, so they exist only when this module is wired.
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::SimQueryHostModule> mSimQueryHost{this};

    bool mCommandsRegistered = false;

    WorldGenerationHealthReporter mHealth;
    bool mHealthRegistered = false;

    Dia::Observation::Metric::Counter* mGenerationsCounter = nullptr;
    Dia::Observation::Metric::Counter* mPublishDroppedCounter = nullptr;
    Dia::Observation::Metric::Counter* mAttemptsCounter = nullptr;
    Dia::Observation::Metric::Counter* mUnsatisfiableCounter = nullptr;
    Dia::Observation::Metric::Counter* mTimeoutsCounter = nullptr;
    Dia::Observation::Metric::Counter* mErrorsCounter = nullptr;
    Dia::Observation::Metric::Counter* mCancelsCounter = nullptr;
    Dia::Observation::Metric::Gauge*   mLastMsGauge = nullptr;
    Dia::Observation::Metric::Gauge*   mRevisionGauge = nullptr;
};

} // namespace CoW
