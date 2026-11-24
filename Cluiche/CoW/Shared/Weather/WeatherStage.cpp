#include <Shared/Weather/WeatherStage.h>

#include <DraoiHexWorld/BiomeRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <Shared/CoWCalendar.h>

namespace CoW {
namespace Weather {

using Draoi::HexWorld::Weather::Season;
using Draoi::HexWorld::Weather::WeatherSimulation;

namespace {
constexpr uint64_t kWeeksPerYear = kWeeksPerSeason * kSeasonsPerYear;
constexpr size_t kInputHistory = 4;   // the announced revisions a late job may still look up
}

void WeatherHooks::Publish(uint32_t revision, std::shared_ptr<const SpinUpInput> input)
{
    std::lock_guard<std::mutex> guard(mLock);
    for (size_t i = 0; i < mInputs.size(); ++i)
    {
        if (mInputs[i].first == revision) { mInputs.erase(mInputs.begin() + i); break; }
    }
    mInputs.emplace_back(revision, std::move(input));
    if (mInputs.size() > kInputHistory) { mInputs.erase(mInputs.begin()); }
    mLatestRevision.store(revision, std::memory_order_release);
}

void WeatherHooks::Invalidate()
{
    std::lock_guard<std::mutex> guard(mLock);
    mInputs.clear();
    mLatestRevision.store(0, std::memory_order_release);
}

std::shared_ptr<const SpinUpInput> WeatherHooks::Find(uint32_t revision) const
{
    std::lock_guard<std::mutex> guard(mLock);
    for (const auto& entry : mInputs)
    {
        if (entry.first == revision) { return entry.second; }
    }
    return nullptr;
}

// Runs on the spin-up job thread: reads only the revision's immutable SpinUpInput. Calendar offset 0: week i of the
// spin-up is week-of-year i % 52 and season (i / 13) % 4, whatever day the live game is on.
bool WeatherHooks::InitialiseSim(WeatherSimulation& sim, const WorldStep::WorldView& world,
                                 WeatherStageBase::CauseCode& cause)
{
    cause = static_cast<uint32_t>(CauseCode::kNone);
    const std::shared_ptr<const SpinUpInput> input = Find(world.revision);
    if (input == nullptr || input->registry == nullptr)
    {
        // Superseded (input dropped) or no biome registry: nothing to build.
        cause = static_cast<uint32_t>(input == nullptr ? CauseCode::kNone : CauseCode::kInitialiseFailed);
        return false;
    }
    if (mLatestRevision.load(std::memory_order_acquire) != world.revision) { return false; }

    if (!sim.Initialise(input->terrain, input->params, *input->registry, nullptr))
    {
        bool anyWater = false;
        for (uint32_t i = 0; i < input->terrain.waterSource.Size(); ++i)
        {
            if (input->terrain.waterSource[i] != Draoi::HexWorld::WaterSource::None) { anyWater = true; break; }
        }
        cause = static_cast<uint32_t>(anyWater || !input->terrain.IsConsistent() ? CauseCode::kInitialiseFailed
                                                                                  : CauseCode::kNoWaterSource);
        return false;
    }

    Dia::ParallelExecutor::SerialExecutor executor;
    const Draoi::HexWorld::Weather::HydrologyInputs noHydro;
    const Draoi::HexWorld::Weather::InfluenceSnapshot noInfluences;
    for (uint32_t week = 0; week < input->spinupWeeks; ++week)
    {
        if (mLatestRevision.load(std::memory_order_acquire) != world.revision) { return false; }   // cancelled
        sim.Step(input->params, static_cast<Season>((week / kWeeksPerSeason) % kSeasonsPerYear),
                 static_cast<uint32_t>(week % kWeeksPerYear), *input->registry, nullptr, noHydro, noInfluences, executor);
        if (onSpinUpWeek) { onSpinUpWeek(week); }
    }
    if (mLatestRevision.load(std::memory_order_acquire) != world.revision) { return false; }
    return true;
}

// Runs inside the base's Capture on SimPU after the queue has been applied. Everything Run will read is latched here
// (or in CaptureSim) so the worker never touches a provider, the parameters or the queue.
bool WeatherHooks::LatchInputs(WeatherLatch& latch, Dia::DeferredStep::StepKey key)
{
    const uint64_t day = static_cast<uint32_t>(key);
    latch.season = static_cast<Season>(SeasonOf(day) % kSeasonsPerYear);
    latch.weekOfYear = static_cast<uint32_t>(WeekOf(day) % kWeeksPerYear);
    if (registry == nullptr)
    {
        DIA_LOG_ERROR(kLogChannel, "WTH: step for day %u has no biome registry, step invalid", static_cast<unsigned>(day));
        return false;
    }
    SnapshotParameters(latch.params);
    latch.hydro.present = false;
    if (hydrologyProvider != nullptr && !hydrologyProvider->GetInputs(latch.hydro)) { latch.hydro.present = false; }
    latch.influences = Draoi::HexWorld::Weather::InfluenceSnapshot();   // Features 7-9 not built
    return true;
}

// Per-cell baseline for the latched season from the provider, one lookup per biome, each clamped to [0, 1].
void WeatherHooks::BuildBaseline(const WeatherSimulation& sim, WeatherLatch& latch)
{
    const uint32_t biomeCount = registry->Count();
    const uint32_t seasonIndex = static_cast<uint32_t>(latch.season);
    if (mClampLogged.size() != static_cast<size_t>(biomeCount) * kSeasonsPerYear)
    {
        mClampLogged.assign(static_cast<size_t>(biomeCount) * kSeasonsPerYear, false);
    }
    std::vector<float> perBiome(biomeCount);
    bool clamped = false;
    for (uint32_t b = 0; b < biomeCount; ++b)
    {
        float v = baselineProvider->GetBaseline(b, latch.season);
        const bool bad = !(v >= 0.0f && v <= 1.0f);   // also catches NaN
        if (bad)
        {
            clamped = true;
            const size_t slot = static_cast<size_t>(b) * kSeasonsPerYear + seasonIndex;
            if (!mClampLogged[slot])
            {
                mClampLogged[slot] = true;
                DIA_LOG_ERROR(kLogChannel, "WTH: baseline for biome %u season %u is %f, clamped to [0, 1]",
                              static_cast<unsigned>(b), static_cast<unsigned>(seasonIndex), static_cast<double>(v));
            }
            v = (v != v) ? 0.5f : (v < 0.0f ? 0.0f : 1.0f);
        }
        perBiome[b] = v;
    }
    const auto& biome = sim.Terrain().biome;
    const uint32_t n = biome.Size();
    latch.baseline.temperature.RemoveAll();
    if (latch.baseline.temperature.Capacity() < n) { latch.baseline.temperature.Reserve(n); }
    for (uint32_t i = 0; i < n; ++i)
    {
        const uint32_t b = static_cast<uint32_t>(biome[i]);
        latch.baseline.temperature.Add(b < biomeCount ? perBiome[b] : 0.5f);
    }
    latch.hasBaseline = true;
    if (clamped)
    {
        mBaselineDegraded = true;
        if (reportDegraded) { reportDegraded(kCauseBaselineClamped); }
    }
    else if (mBaselineDegraded)
    {
        mBaselineDegraded = false;
        if (clearDegraded) { clearDegraded(); }
    }
}

void WeatherHooks::CaptureSim(WeatherSimulation& working, WeatherLatch& latch)
{
    const Draoi::HexWorld::Weather::SeasonalBaselineInput* baseline = nullptr;
    if (baselineProvider != nullptr)
    {
        mFallbackLogged = false;
        BuildBaseline(working, latch);
        baseline = &latch.baseline;
    }
    else
    {
        latch.hasBaseline = false;
        if (!mFallbackLogged)
        {
            mFallbackLogged = true;
            DIA_LOG_INFO(kLogChannel, "WTH: no biome baseline provider set, using the sidecar baseline table");
        }
        mBaselineDegraded = true;
        if (reportDegraded) { reportDegraded(kCauseFallbackBaseline); }
    }
    working.Capture(latch.params, latch.season, latch.weekOfYear, *registry, baseline, latch.hydro, latch.influences);
}

void WeatherHooks::ApplyEntry(WeatherSimulation&, WeatherLatch&, const WeatherEntry&) {}

void WeatherHooks::RunStep(WeatherSimulation& working, const WeatherLatch&, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    working.Run(executor);
}

void WeatherHooks::CommitSim(WeatherSimulation& sim, uint32_t stepDay)
{
    sim.Commit();
    sim.SetCommittedWeek(WeekOf(stepDay));
}

void WeatherHooks::PublishCommit(const WeatherSimulation&, uint32_t stepDay)
{
    if (publishWeather)
    {
        publishWeather(WeekOf(stepDay), static_cast<Season>(SeasonOf(stepDay) % kSeasonsPerYear));
    }
}

WeatherStage::WeatherStage()
    : Detail::HooksHolder()
    , WeatherStageBase(hooks, World::kWeather, kLogChannel, &CauseTextRaw)
{
    hooks.reportDegraded = [this](const char* cause) { ReportDegradedCause(cause); };
    hooks.clearDegraded = [this]() { ClearDegradedCause(); };
}

void WeatherStage::OnWorldComplete(uint32_t revision)
{
    if (mSource == nullptr)
    {
        DIA_LOG_ERROR(kLogChannel, "WTH: world %u announced but no world source is set", static_cast<unsigned>(revision));
        return;
    }
    WorldStep::WorldView view;
    view.revision = revision;
    std::shared_ptr<SpinUpInput> input = std::make_shared<SpinUpInput>();
    if (!mSource->GetWorld(view, input->terrain) || !view.present)
    {
        DIA_LOG_WARNING(kLogChannel, "WTH: world %u announced but the world source has no world for it, ignored",
                        static_cast<unsigned>(revision));
        return;
    }
    view.revision = revision;
    input->params = hooks.params;
    input->registry = hooks.registry;
    input->spinupWeeks = hooks.spinupWeeks;
    hooks.Publish(revision, input);
    OnWorldGenerationComplete(view);
}

void WeatherStage::OnWorldFailed()
{
    hooks.Invalidate();
    OnWorldGenerationFailed();
}

WeatherSnapshot WeatherStage::GetSnapshot() const
{
    WeatherSnapshot snap;
    const WeatherSimulation* sim = ReadableSim();
    if (sim == nullptr || !sim->IsInitialised()) { return snap; }
    snap.fields = &sim->Fields();
    snap.cellCount = snap.fields->CellCount();
    snap.week = sim->GetCommittedWeek();
    return snap;
}

bool WeatherStage::RunNextJob()
{
    if (mQueued.empty()) { return false; }
    std::function<void()> job = std::move(mQueued.front());
    mQueued.erase(mQueued.begin());
    job();
    return true;
}

void WeatherStage::LaunchJob(uint32_t revision, uint32_t generation, const WorldStep::WorldView& world)
{
    switch (mMode)
    {
    case LaunchMode::kSynchronous:
        RunJob(revision, generation, world);
        break;
    case LaunchMode::kManual:
        mQueued.push_back([this, revision, generation, world]() { RunJob(revision, generation, world); });
        break;
    default:
        WeatherStageBase::LaunchJob(revision, generation, world);
        break;
    }
}

} // namespace Weather
} // namespace CoW
