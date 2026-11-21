#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <Shared/WorldGeneration/SharedWorldHexGrid.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/StrataAssigner.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include "Modules/WorldGen/WorldGenerationMessages.h"
#include "Modules/MessageBusHostModule.h"
#include "Modules/Plumbing/RandomStreamModule.h"
#include "Modules/SimQueryHostModule.h"
#include <cstdint>
#include <memory>
#include <string>

namespace CoW {

namespace World { class BiomeRegistry; }   // full definition: Shared/World/BiomeRegistry.h (pulls windows.h; kept out of this header)

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

#ifdef DIA_DEBUG
    // The one hex grid every cell-layer registrant of the world registers against (S-1 view, A4, A1). Null until the
    // first Update after DoStart. Rebuilt in place when the world dimensions change (stable CellGridDesc address).
    // Debug builds only: every registrant is debug-only, so Release neither builds nor references it.
    const WorldGeneration::WorldHexGrid* GetHexGrid() const { return mHexGrid.Get(); }
    // For a registrant that choreographs layer (un)registration around a size change (see SharedWorldHexGrid).
    WorldGeneration::SharedWorldHexGrid& HexGridOwner() { return mHexGrid; }
#endif
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
    // Strata assigners (strata.json selects one per set by type). A game registers its own before the strata load; the
    // built-in coast_cost_bands is always present.
    WorldGeneration::StrataAssignerRegistry& GetStrataAssignerRegistry() { return mStrataAssigners; }
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

    // Terrain rules sidecar (a Dia parameter file, catalogue alias json.terrain_rules_schema). Where it comes from:
    //   - SetRulesSidecarText (a host supplied the text; tests): always wins.
    //   - config key "rules_sidecar_alias" (production): the PathStore alias the catalogue registers for the asset
    //     (SimPU read, user-approved exception to the no-statics rule 2026-11-20, precedent Scene2DModule). Aliases are
    //     registered by AssetServiceModule::DoStart, after OnConfigure, so the load and the rest of the config
    //     (seed / size / preset / regenerate_on_start) are deferred to this module's DoStart. An unregistered alias or
    //     unreadable file fails (Error log, health Failing naming the alias, "rules_not_loaded"); there is NO fallback
    //     to the default path when an alias was configured.
    //   - neither: kDefaultRulesSidecarPath (the test / dev default), read at the start of OnConfigure.
    // A failure logs an Error, puts health in Failing once DoStart runs, and every generation entry point of the
    // session refuses with "rules_not_loaded". There is no compiled fallback.
    static constexpr const char* kDefaultRulesSidecarPath = "assets/global/misc/terrain_rules.schema.json";
    void SetRulesSidecarText(const std::string& text) { mRulesSidecarText = text; mRulesSidecarTextSet = true; }
    void SetRulesSidecarPath(const std::string& path) { mRulesSidecarPath = path; }
    const std::string& GetRulesSidecarPath() const { return mRulesSidecarPath; }
    // Loads the sidecar into the session now, once (OnConfigure and DoStart call it too; later calls are no-ops).
    void LoadRulesSidecar();
    bool AreRulesLoaded() const { return mSession.RulesLoaded(); }
    // The first load failure (empty when the rules loaded): ParamError key and reason.
    const std::string& GetRulesErrorKey() const { return mRulesErrorKey; }
    const std::string& GetRulesErrorReason() const { return mRulesErrorReason; }

    // Biome registry (spec biome-registry.md, Ownership): owned here, loaded once in DoStart, immutable afterwards (no
    // reload). Where the JSON comes from (first match wins):
    //   - SetBiomesText (a host supplied the text; tests),
    //   - SetBiomesPath (a plain file path; tests / dev),
    //   - otherwise the catalogue alias kBiomesAlias ("json.biomes") resolved through PathStore (same user-approved
    //     read as the rules sidecar; aliases are registered before DoStart).
    // A failure logs an Error, puts health in Failing naming the field ("biomes_load_failed:<field>"), and every
    // generation entry point of the session refuses with "biomes_not_loaded".
    static constexpr const char* kBiomesAlias = "json.biomes";
    void SetBiomesText(const std::string& text) { mBiomesText = text; mBiomesTextSet = true; }
    void SetBiomesPath(const std::string& path) { mBiomesPath = path; }
    // Stratum table (strata.json + sidecar strata.schema.json), loaded before the biomes (which resolve stratum names
    // against it). Sources, first match: SetStrataText (tests), SetStrataPath (the sidecar is read from the same
    // directory), the directory of SetBiomesPath, else the catalogue aliases kStrataAlias / kStrataSidecarAlias.
    static constexpr const char* kStrataAlias = "json.strata";
    static constexpr const char* kStrataSidecarAlias = "json.strata_schema";
    void SetStrataText(const std::string& strata, const std::string& sidecar) { mStrataText = strata; mStrataSidecarText = sidecar; mStrataTextSet = true; }
    void SetStrataPath(const std::string& path) { mStrataPath = path; }
    // Guarantee spec (guarantees.json + sidecar guarantees.schema.json), loaded after the strata (entries name strata) and
    // installed into the session. Sources, first match: SetGuaranteesText (tests), SetGuaranteesPath (sidecar from the
    // same directory), guarantees.json beside the biomes path when it exists, the catalogue aliases kGuaranteesAlias /
    // kGuaranteesSidecarAlias when registered. With no source the spec stays empty (only registry guarantees and the
    // layout validate). A source that cannot be read or parsed fails like the strata: field "guarantees.<entry>".
    static constexpr const char* kGuaranteesAlias = "json.guarantees";
    static constexpr const char* kGuaranteesSidecarAlias = "json.guarantees_schema";
    void SetGuaranteesText(const std::string& text, const std::string& sidecar) { mGuaranteesText = text; mGuaranteesSidecarText = sidecar; mGuaranteesTextSet = true; }
    void SetGuaranteesPath(const std::string& path) { mGuaranteesPath = path; }
    const World::BiomeRegistry& GetBiomeRegistry() const { return *mBiomes; }
    bool AreBiomesLoaded() const { return mBiomesLoaded; }
    // The first biome load failure (empty when loaded): RegistryError field, id, row and reason / the read failure.
    const std::string& GetBiomesErrorField() const { return mBiomesErrorField; }
    const std::string& GetBiomesErrorReason() const { return mBiomesErrorReason; }

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
    // Launch preset (Stage Catalogue F2): applies seed, size and preset from the override over the manifest config.
    void OnStageLaunch(const char* overridesJson) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void ApplySeedAndSize(const Json::Value& root);
    // The config body of OnConfigure (loads the rules first); deferred to DoStart when the rules come from an alias.
    void ApplyConfig(const char* configJson);
    void ReportRulesFailure();
    bool LoadStrata(World::StratumTable& strata);
    bool LoadGuarantees(const World::StratumTable& strata);
    void LoadBiomes();
    void ReportBiomesFailure();
    void ApplyHandoffPreset();
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
    WorldGeneration::StrataAssignerRegistry mStrataAssigners;
#ifdef DIA_DEBUG
    WorldGeneration::SharedWorldHexGrid mHexGrid;
#endif
    int  mGenerationHolds = 0;
    bool mStartDeferral = true;      // skip the first DoUpdate: dependants started in the same pass have registered by then
    std::string mRulesSidecarPath = kDefaultRulesSidecarPath;
    std::string mRulesSidecarText;
    bool mRulesSidecarTextSet = false;
    bool mRulesAttempted = false;
    std::string mRulesSidecarAlias;   // "rules_sidecar_alias": resolved through PathStore when given
    bool mRulesAliasGiven = false;
    bool mConfigDeferred = false;     // OnConfigure waited for the alias; DoStart applies mConfigPending
    std::string mConfigPending;
    std::string mPendingLaunchOverrides;
    std::string mRulesErrorKey;
    std::string mRulesErrorReason;
    std::unique_ptr<World::BiomeRegistry> mBiomes;
    std::string mStrataText;
    std::string mStrataSidecarText;
    bool mStrataTextSet = false;
    std::string mStrataPath;
    std::string mGuaranteesText;
    std::string mGuaranteesSidecarText;
    bool mGuaranteesTextSet = false;
    std::string mGuaranteesPath;
    std::string mBiomesText;
    bool mBiomesTextSet = false;
    std::string mBiomesPath;
    bool mBiomesLoaded = false;
    std::string mBiomesErrorField;
    std::string mBiomesErrorReason;
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
