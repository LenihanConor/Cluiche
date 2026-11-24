// The weather world-step stage: WorldStepStageBase instantiated over the Draoi weather simulation. Task 2 fills in world
// ingest and spin-up (InitialiseSim); Tasks 3-4 fill in the per-step hooks.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-module.md
#pragma once

#include <DiaDeferredStep/IStepLauncher.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>
#include <Shared/Weather/BiomeSeasonalBaseline.h>
#include <Shared/Weather/HydrologyInputsProvider.h>
#include <Shared/Weather/WeatherCause.h>
#include <Shared/Weather/WeatherEntry.h>
#include <Shared/Weather/WeatherLatch.h>
#include <Shared/Weather/WeatherParameterStore.h>
#include <Shared/Weather/WeatherPreset.h>
#include <Shared/Weather/WeatherSideStore.h>
#include <Shared/Weather/WeatherStateRecord.h>
#include <Shared/WorldStep/WorldStepStageBase.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace CoW {
namespace Weather {

typedef WorldStep::WorldStepStageBase<Draoi::HexWorld::Weather::WeatherSimulation, WeatherEntry, WeatherLatch>
    WeatherStageBase;

constexpr const char* kLogChannel = "WTH";
constexpr uint32_t kDefaultSpinupWeeks = 52;
constexpr uint32_t kMaxRegionCells = 65536;   // bound on one region forcing's cell list

// Where the stage reads the world from. The module adapts WorldGenerationModule; tests hand-build terrain.
// SimPU only. Fills `view` (width, height, present; revision is pre-set by the caller) and `terrain`; false = no world.
class IWeatherWorldSource
{
public:
    virtual ~IWeatherWorldSource() {}
    virtual bool GetWorld(WorldStep::WorldView& view, Draoi::HexWorld::Weather::WeatherTerrain& terrain) = 0;
};

// Where the stage gets the shared step launcher for a capped spin-up (kCappedLauncher). The module adapts
// DeferredStepModule::GetLauncher. SimPU only. The launcher can be swapped while idle, so the stage calls Get() at each
// launch and each reap and never keeps the pointer across frames (except to detect a swap). Null = no launcher.
class ILauncherSource
{
public:
    virtual ~ILauncherSource() {}
    virtual Dia::DeferredStep::IStepLauncher* Get() = 0;
};

// What the spin-up job reads: an immutable copy taken on SimPU when the world was announced, so the job never touches
// a module or a world pointer.
struct SpinUpInput
{
    Draoi::HexWorld::Weather::WeatherTerrain terrain;
    Draoi::HexWorld::Weather::WeatherParameters params;
    const Draoi::HexWorld::BiomeRegistry* registry = nullptr;
    uint32_t spinupWeeks = 0;
};

// A read-only view over the committed fields. cellCount 0 (fields null) before the stage has a sim for the target world.
struct WeatherSnapshot
{
    uint32_t cellCount = 0;
    uint64_t week = 0;
    const Draoi::HexWorld::Weather::WeatherFields* fields = nullptr;
};

// The hook implementation. Constructed before the base (see HooksHolder) so the base can bind to it.
class WeatherHooks : public WeatherStageBase::Hooks
{
public:
    bool InitialiseSim(Draoi::HexWorld::Weather::WeatherSimulation& sim, const WorldStep::WorldView& world,
                       WeatherStageBase::CauseCode& cause) override;
    bool LatchInputs(WeatherLatch& latch, Dia::DeferredStep::StepKey key) override;
    void ApplyEntry(Draoi::HexWorld::Weather::WeatherSimulation& working, WeatherLatch& latch,
                    const WeatherEntry& entry) override;
    void RunStep(Draoi::HexWorld::Weather::WeatherSimulation& working, const WeatherLatch& latch,
                 Dia::ParallelExecutor::IParallelExecutor& executor) override;
    void PublishCommit(const Draoi::HexWorld::Weather::WeatherSimulation& committed, uint32_t stepDay) override;
    void CaptureSim(Draoi::HexWorld::Weather::WeatherSimulation& working, WeatherLatch& latch) override;
    void CommitSim(Draoi::HexWorld::Weather::WeatherSimulation& sim, uint32_t stepDay) override;

    void OnStopExtra() override { side.Reset(); }

    // Restore step 4 (weather-save): DecodeRecord against saveSpec/registry, then sim.RestoreCommitted. false -> the base
    // discards the restore. ExtraCountMatches keeps the base default (true): weather has no extra counts.
    bool RestoreCommitted(Draoi::HexWorld::Weather::WeatherSimulation& sim, const Json::Value& record) override;

    // The fields and caps a saved record is decoded against (not owned; set through WeatherStage::SetSaveSpec).
    const WorldStep::RecordSpec* saveSpec = nullptr;

    // The step's parameters: a ParamBinding snapshot of the loaded sidecar. When no sidecar was loaded (tests that hand
    // the stage a parameter struct through SetParameters) a copy of `params`.
    void SnapshotParameters(Draoi::HexWorld::Weather::WeatherParameters& out) const
    {
        if (store.IsLoaded()) { store.Snapshot(out); } else { out = params; }
    }

    WeatherParameterStore store;
    WeatherSideStore side;
    // Called after a drained entry changed the parameter values: (reloadOrPack). The stage refreshes the params hash and,
    // for a reload or pack, tells the base the saved-hash warning no longer applies.
    std::function<void(bool)> paramsChanged;

    IBiomeSeasonalBaseline* baselineProvider = nullptr;
    IHydrologyInputsProvider* hydrologyProvider = nullptr;

    // Seams the stage wires: health cause set/clear and the WeatherUpdated publisher (week, season).
    std::function<void(const char*)> reportDegraded;
    std::function<void()> clearDegraded;
    std::function<void(uint64_t, Draoi::HexWorld::Weather::Season)> publishWeather;

    // SimPU: the input for a revision becomes current (older ones are dropped beyond a short history), or none is.
    void Publish(uint32_t revision, std::shared_ptr<const SpinUpInput> input);
    void Invalidate();

    // Settings copied into each world's SpinUpInput when it is announced (SimPU only).
    Draoi::HexWorld::Weather::WeatherParameters params;
    const Draoi::HexWorld::BiomeRegistry* registry = nullptr;
    uint32_t spinupWeeks = kDefaultSpinupWeeks;

    // Test seam: called on the job thread after each spin-up week with the 0-based week index just completed.
    std::function<void(uint32_t)> onSpinUpWeek;

private:
    std::shared_ptr<const SpinUpInput> Find(uint32_t revision) const;

    void BuildBaseline(const Draoi::HexWorld::Weather::WeatherSimulation& sim, WeatherLatch& latch);

    std::vector<bool> mClampLogged;        // [biome * 4 + season], sized by registry Count()
    bool mFallbackLogged = false;
    bool mProviderLogged = false;
    bool mBaselineDegraded = false;

    mutable std::mutex mLock;
    std::vector<std::pair<uint32_t, std::shared_ptr<const SpinUpInput>>> mInputs;
    std::atomic<uint32_t> mLatestRevision{0};   // 0 = none; a job whose revision differs has been superseded
};

namespace Detail {
struct HooksHolder
{
    WeatherHooks hooks;
};
} // namespace Detail

class WeatherStage : private Detail::HooksHolder, public WeatherStageBase
{
public:
    // How a spin-up job runs. kThread is the default (the base's worker thread); kSynchronous runs the job inside
    // OnWorldComplete; kManual queues it until RunNextJob (tests that control job timing); kJobSystem hands the job to the
    // submitter (production: JobSystemService TrySubmit, tag 'world') and runs it inline with one Warning when the
    // submitter is missing or refuses (a job is never dropped). kCappedLauncher runs the job as a deferred step on the
    // shared launcher (the same strict worker cap as the weekly step); with no launcher it falls back to the kJobSystem
    // path with one Warning.
    enum class LaunchMode : uint8_t { kThread, kSynchronous, kManual, kJobSystem, kCappedLauncher };
    // Takes ownership of the function; true = accepted (it will run exactly once on another thread), false = refused
    // (the function must not have run).
    typedef std::function<bool(std::function<void()>)> JobSubmitter;

    WeatherStage();
    ~WeatherStage() override
    {
        hooks.Invalidate();   // a running spin-up job sees it between weeks, so the base joins promptly
        WaitForPoolJobs();
        DrainLauncherTokens();
    }

    // Polls the spin-up slot (base) and then releases the launcher tokens of finished capped spin-ups. SimPU only.
    void Update();
    // Test seam: capped spin-up launches not yet released.
    uint32_t LiveLauncherTokens() const;
    void SetLauncherSource(ILauncherSource* source) { mLauncherSource = source; }

    void SetLaunchMode(LaunchMode mode) { mMode = mode; }
    void SetJobSubmitter(JobSubmitter submitter) { mSubmitter = std::move(submitter); }
    size_t QueuedJobCount() const { return mQueued.size(); }
    bool RunNextJob();   // kManual: runs the oldest queued job; false if none

    // Spin-up settings (Task 4 loads the parameters from the sidecar; until then the module leaves the defaults).
    void SetWorldSource(IWeatherWorldSource* source) { mSource = source; }
    void SetParameters(const Draoi::HexWorld::Weather::WeatherParameters& params) { hooks.params = params; }
    void SetRegistry(const Draoi::HexWorld::BiomeRegistry* registry) { hooks.registry = registry; }
    bool HasRegistry() const { return hooks.registry != nullptr; }
    const Draoi::HexWorld::BiomeRegistry* GetRegistry() const { return hooks.registry; }
    // The record spec used both for pending-blob validation (base) and for decoding a committed record in Restore.
    void SetSaveSpec(const WorldStep::RecordSpec* spec) { hooks.saveSpec = spec; SetRecordSpec(spec); }
    void SetSpinupWeeks(uint32_t weeks) { hooks.spinupWeeks = weeks; }
    // Providers are borrowed (null = none); read only inside Capture on SimPU.
    void SetBaselineProvider(IBiomeSeasonalBaseline* provider) { hooks.baselineProvider = provider; }
    void SetHydrologyProvider(IHydrologyInputsProvider* provider) { hooks.hydrologyProvider = provider; }
    // Called after commit with the step's week and season; the module posts WeatherUpdated on the bus.
    void SetPublishSeam(std::function<void(uint64_t, Draoi::HexWorld::Weather::Season)> fn) { hooks.publishWeather = std::move(fn); }
    void SetSpinUpWeekObserver(std::function<void(uint32_t)> observer) { hooks.onSpinUpWeek = std::move(observer); }

    // Loads the parameter sidecar (strict, against the registry's biome ids, active packs applied). Any fault puts the
    // stage in Failing naming the dotted key (cause parameter_fault), permanently: it never becomes ready, no fallback.
    // Needs SetRegistry first. SimPU, before the first world is announced.
    bool LoadParameters(IWeatherSidecarSource& source, const std::vector<Json::Value>& packs, ParamError& err);
    const WeatherParameterStore& Parameters() const { return hooks.store; }
    uint64_t ParametersHash() const { return hooks.store.Hash(); }

    // ---- queue API (SimPU, any time; entries apply in arrival order at the next Capture, before the step latches) ----
    // false = refused: invalid (err names the key; nothing queued) or the 128-entry queue is full (one Warning, Degraded).
    bool QueueParameterSet(const char* dottedKey, float value, ParamError& err);
    bool QueueParameterOverrides(const Json::Value& pack, ParamError& err);   // lenient pack, atomic when drained
    bool QueueReload();
    bool QueueForcing(const Draoi::HexWorld::Weather::WeatherForcing& forcing);
    bool QueueRegionForcing(const Draoi::HexWorld::Weather::WeatherForcing& forcing,
                            const Dia::Core::Containers::DynamicArray<uint32_t>& cells);
    bool QueueBiomeChange(uint32_t cell, Draoi::HexWorld::BiomeIndex biome);
    bool QueuePreset(const WeatherPreset& preset);
    bool QueueStateReplace(const WeatherStateRecord& record);
    bool QueueClearWindOverrides();

    // A world revision was announced: copies the world into a SpinUpInput and starts the spin-up. SimPU only.
    void OnWorldComplete(uint32_t revision);
    // The world failed: drops every held input and the sims. SimPU only.
    void OnWorldFailed();

    WeatherSnapshot GetSnapshot() const;

protected:
    // First step of Stop: cancels a running spin-up job (it checks between weeks) before the base joins it.
    void UnregisterStage() override
    {
        hooks.Invalidate();
        WaitForPoolJobs();   // pool jobs are not the base's mJob: Stop must not return while one still touches the stage
        DrainLauncherTokens();   // same for capped-launcher jobs: cancelled between weeks, then waited and released
    }
    void LaunchJob(uint32_t revision, uint32_t generation, const WorldStep::WorldView& world) override;

private:
    bool Push(const WeatherEntry& entry, bool usedSlot);   // Enqueue; hands the side-store slot back when refused

    void WaitForPoolJobs();
    void LaunchViaPool(uint32_t revision, uint32_t generation, const WorldStep::WorldView& world, bool launcherMissing);
    void LaunchViaLauncher(Dia::DeferredStep::IStepLauncher& launcher, uint32_t revision, uint32_t generation,
                           const WorldStep::WorldView& world);
    void ReapLauncherTokens();    // releases finished tokens (and drops those of a swapped-out launcher)
    void DrainLauncherTokens();   // waits for every live token, then releases it
    void FreeLauncherSlot(uint32_t index);

    class SpinUpStep;   // IDeferredStep adapter over RunJob (defined in the .cpp)
    static constexpr uint32_t kMaxLauncherTokens = 4;
    struct LiveToken
    {
        SpinUpStep* step = nullptr;   // owned; null = slot free
        Dia::DeferredStep::LaunchToken token;
        Dia::DeferredStep::IStepLauncher* launcher = nullptr;   // the launcher that issued the token
        uint64_t sequence = 0;
    };
    LiveToken mLive[kMaxLauncherTokens];
    uint64_t mLiveSequence = 0;
    ILauncherSource* mLauncherSource = nullptr;

    IWeatherWorldSource* mSource = nullptr;
    JobSubmitter mSubmitter;
    std::mutex mPoolLock;
    std::condition_variable mPoolCv;
    uint32_t mPoolActive = 0;   // pool jobs submitted and not yet finished (guarded by mPoolLock)
    LaunchMode mMode = LaunchMode::kThread;
    std::vector<std::function<void()>> mQueued;
};

} // namespace Weather
} // namespace CoW
