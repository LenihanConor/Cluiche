// The weather world-step stage: WorldStepStageBase instantiated over the Draoi weather simulation. Task 2 fills in world
// ingest and spin-up (InitialiseSim); Tasks 3-4 fill in the per-step hooks.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-module.md
#pragma once

#include <DraoiHexWorldWeather/WeatherSimulation.h>
#include <Shared/Weather/WeatherCause.h>
#include <Shared/Weather/WeatherEntry.h>
#include <Shared/Weather/WeatherLatch.h>
#include <Shared/WorldStep/WorldStepStageBase.h>

#include <atomic>
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

// Where the stage reads the world from. The module adapts WorldGenerationModule; tests hand-build terrain.
// SimPU only. Fills `view` (width, height, present; revision is pre-set by the caller) and `terrain`; false = no world.
class IWeatherWorldSource
{
public:
    virtual ~IWeatherWorldSource() {}
    virtual bool GetWorld(WorldStep::WorldView& view, Draoi::HexWorld::Weather::WeatherTerrain& terrain) = 0;
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
    // How a spin-up job runs. kThread is the production default (the base's worker thread); kSynchronous runs the job
    // inside OnWorldComplete; kManual queues it until RunNextJob (tests that control job timing).
    enum class LaunchMode : uint8_t { kThread, kSynchronous, kManual };

    WeatherStage();

    void SetLaunchMode(LaunchMode mode) { mMode = mode; }
    size_t QueuedJobCount() const { return mQueued.size(); }
    bool RunNextJob();   // kManual: runs the oldest queued job; false if none

    // Spin-up settings (Task 4 loads the parameters from the sidecar; until then the module leaves the defaults).
    void SetWorldSource(IWeatherWorldSource* source) { mSource = source; }
    void SetParameters(const Draoi::HexWorld::Weather::WeatherParameters& params) { hooks.params = params; }
    void SetRegistry(const Draoi::HexWorld::BiomeRegistry* registry) { hooks.registry = registry; }
    void SetSpinupWeeks(uint32_t weeks) { hooks.spinupWeeks = weeks; }
    void SetSpinUpWeekObserver(std::function<void(uint32_t)> observer) { hooks.onSpinUpWeek = std::move(observer); }

    // A world revision was announced: copies the world into a SpinUpInput and starts the spin-up. SimPU only.
    void OnWorldComplete(uint32_t revision);
    // The world failed: drops every held input and the sims. SimPU only.
    void OnWorldFailed();

    WeatherSnapshot GetSnapshot() const;

protected:
    void LaunchJob(uint32_t revision, uint32_t generation, const WorldStep::WorldView& world) override;

private:
    IWeatherWorldSource* mSource = nullptr;
    LaunchMode mMode = LaunchMode::kThread;
    std::vector<std::function<void()>> mQueued;
};

} // namespace Weather
} // namespace CoW
