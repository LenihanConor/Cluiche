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

// Tasks 3-4.
bool WeatherHooks::LatchInputs(WeatherLatch&, Dia::DeferredStep::StepKey)
{
    return true;
}

void WeatherHooks::ApplyEntry(WeatherSimulation&, WeatherLatch&, const WeatherEntry&) {}

void WeatherHooks::RunStep(WeatherSimulation&, const WeatherLatch&, Dia::ParallelExecutor::IParallelExecutor&) {}

void WeatherHooks::PublishCommit(const WeatherSimulation&, uint32_t) {}

WeatherStage::WeatherStage()
    : Detail::HooksHolder()
    , WeatherStageBase(hooks, World::kWeather, kLogChannel, &CauseTextRaw)
{
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
