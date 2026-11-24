#include <Shared/Weather/WeatherStage.h>

#include <DraoiHexWorld/BiomeRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <Shared/CoWCalendar.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

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
    DIA_TRACE_ZONE("WeatherStage::SpinUp", Dia::Observation::Trace::Category::kNone);
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

    DIA_LOG_INFO(kLogChannel, "WTH: spin-up start for world %u, %u weeks", static_cast<unsigned>(world.revision),
                 static_cast<unsigned>(input->spinupWeeks));
    const auto spinUpStart = std::chrono::steady_clock::now();
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
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - spinUpStart).count();
    DIA_LOG_INFO(kLogChannel, "WTH: spin-up finished for world %u in %.1f ms", static_cast<unsigned>(world.revision), ms);
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
    DIA_TRACE_ZONE("WeatherStage::Capture", Dia::Observation::Trace::Category::kNone);
    const Draoi::HexWorld::Weather::SeasonalBaselineInput* baseline = nullptr;
    if (baselineProvider != nullptr)
    {
        if (mFallbackLogged || !mProviderLogged)
        {
            DIA_LOG_INFO(kLogChannel, "WTH: using the biome baseline provider");
            mProviderLogged = true;
        }
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

// Runs inside the base's Capture on SimPU, before LatchInputs: a queued set is visible to this step's snapshot.
void WeatherHooks::ApplyEntry(WeatherSimulation& working, WeatherLatch&, const WeatherEntry& entry)
{
    ParamError err;
    switch (entry.kind)
    {
    case EntryKind::kSetParam:
        if (store.Set(entry.key, Json::Value(static_cast<double>(entry.value)), err))
        {
            if (paramsChanged) { paramsChanged(false); }
        }
        else { DIA_LOG_ERROR(kLogChannel, "WTH: queued set of '%s' refused at apply: %s", err.key, err.reason); }
        break;
    case EntryKind::kParamOverrides:
        if (store.ApplyPack(side.Get(entry.slot).pack, err))
        {
            if (paramsChanged) { paramsChanged(true); }
        }
        else { DIA_LOG_ERROR(kLogChannel, "WTH: queued parameter pack rejected at '%s': %s", err.key, err.reason); }
        break;
    case EntryKind::kReload:
        if (store.Reload(err))
        {
            DIA_LOG_INFO(kLogChannel, "WTH: parameter sidecar reloaded");
            if (paramsChanged) { paramsChanged(true); }
        }
        else { DIA_LOG_ERROR(kLogChannel, "WTH: parameter reload failed at '%s': %s; previous values kept", err.key, err.reason); }
        break;
    case EntryKind::kForcing:
        if (!working.AddForcing(entry.forcing)) { DIA_LOG_WARNING(kLogChannel, "WTH: forcing refused by the simulation"); }
        break;
    case EntryKind::kRegionForcing:
    {
        const std::vector<uint32_t>& cells = side.Get(entry.slot).cells;
        Dia::Core::Containers::DynamicArray<uint32_t> set;
        set.Reserve(static_cast<uint32_t>(cells.size()));
        for (uint32_t c : cells) { set.Add(c); }
        const Draoi::HexWorld::Weather::CellSetHandle handle = working.RegisterCellSet(set);
        if (handle == Draoi::HexWorld::Weather::kNoCellSet)
        {
            DIA_LOG_WARNING(kLogChannel, "WTH: region forcing refused: cell set rejected (out of range or store full)");
            break;
        }
        Draoi::HexWorld::Weather::WeatherForcing forcing = entry.forcing;
        forcing.cellSet = handle;
        if (!working.AddForcing(forcing)) { DIA_LOG_WARNING(kLogChannel, "WTH: region forcing refused by the simulation"); }
        break;
    }
    case EntryKind::kBiomeChange:
        if (!working.SetCellBiome(entry.cell, entry.biome))
        {
            DIA_LOG_WARNING(kLogChannel, "WTH: biome change for cell %u refused by the simulation", static_cast<unsigned>(entry.cell));
        }
        break;
    case EntryKind::kPreset:
        // Plan Feature 6 owns WeatherPreset and ApplyPreset; the kind and slot are in place for it.
        DIA_LOG_WARNING(kLogChannel, "WTH: preset '%s' drained but not applied (preset support is Feature 6)",
                        side.Get(entry.slot).preset.id);
        break;
    case EntryKind::kStateReplace:
        DIA_LOG_WARNING(kLogChannel, "WTH: state replace drained but not applied (state restore is Feature 4)");
        break;
    case EntryKind::kClearWindOverrides:
        working.ClearWindOverrides();
        break;
    default:
        break;
    }
}

void WeatherHooks::RunStep(WeatherSimulation& working, const WeatherLatch&, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    working.Run(executor);
}

void WeatherHooks::CommitSim(WeatherSimulation& sim, uint32_t stepDay)
{
    DIA_TRACE_ZONE("WeatherStage::Commit", Dia::Observation::Trace::Category::kNone);
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
    hooks.paramsChanged = [this](bool reloadOrPack) {
        SetCurrentParamsHash(hooks.store.Hash());
        if (reloadOrPack) { NotifyParamsReloaded(); }
    };
}

bool WeatherStage::LoadParameters(IWeatherSidecarSource& source, const std::vector<Json::Value>& packs, ParamError& err)
{
    if (hooks.registry == nullptr)
    {
        err.Set("registry", "no biome registry to load the parameter sidecar against");
    }
    else
    {
        hooks.store.SetActivePacks(packs);
        if (hooks.store.Load(source, *hooks.registry, err))
        {
            SetCurrentParamsHash(hooks.store.Hash());
            DIA_LOG_INFO(kLogChannel, "WTH: parameter sidecar loaded");
            return true;
        }
    }
    FailStage(err.key, static_cast<uint32_t>(::CoW::Weather::CauseCode::kParameterFault));
    DIA_LOG_ERROR(kLogChannel, "WTH: parameter fault at '%s': %s (%s); stage is Failing, no fallback", err.key, err.reason,
                  ::CoW::Weather::CauseText(::CoW::Weather::CauseCode::kParameterFault));
    return false;
}

bool WeatherStage::Push(const WeatherEntry& entry, bool usedSlot)
{
    if (Enqueue(entry)) { return true; }
    if (usedSlot) { hooks.side.Rollback(); }
    return false;
}

bool WeatherStage::QueueParameterSet(const char* dottedKey, float value, ParamError& err)
{
    err.Clear();
    if (dottedKey == nullptr || std::strlen(dottedKey) >= kEntryKeyCapacity)
    {
        err.Set(dottedKey != nullptr ? dottedKey : "", "parameter key is missing or too long");
        return false;
    }
    if (!hooks.store.CheckSet(dottedKey, Json::Value(static_cast<double>(value)), err)) { return false; }
    WeatherEntry entry;
    entry.kind = EntryKind::kSetParam;
    std::snprintf(entry.key, sizeof(entry.key), "%s", dottedKey);
    entry.value = value;
    if (!Push(entry, false))
    {
        err.Set(dottedKey, "mutation queue is full");
        return false;
    }
    return true;
}

bool WeatherStage::QueueParameterOverrides(const Json::Value& pack, ParamError& err)
{
    err.Clear();
    if (!pack.isObject())
    {
        err.Set("pack", "a parameter pack must be a JSON object");
        return false;
    }
    WeatherEntry entry;
    entry.kind = EntryKind::kParamOverrides;
    entry.slot = hooks.side.Acquire();
    hooks.side.Get(entry.slot).pack = pack;
    if (!Push(entry, true))
    {
        err.Set("pack", "mutation queue is full");
        return false;
    }
    return true;
}

bool WeatherStage::QueueReload()
{
    WeatherEntry entry;
    entry.kind = EntryKind::kReload;
    return Push(entry, false);
}

bool WeatherStage::QueueForcing(const Draoi::HexWorld::Weather::WeatherForcing& forcing)
{
    WeatherEntry entry;
    entry.kind = EntryKind::kForcing;
    entry.forcing = forcing;
    return Push(entry, false);
}

bool WeatherStage::QueueRegionForcing(const Draoi::HexWorld::Weather::WeatherForcing& forcing,
                                      const Dia::Core::Containers::DynamicArray<uint32_t>& cells)
{
    if (cells.Size() == 0 || cells.Size() > kMaxRegionCells) { return false; }
    WeatherEntry entry;
    entry.kind = EntryKind::kRegionForcing;
    entry.forcing = forcing;
    entry.slot = hooks.side.Acquire();
    std::vector<uint32_t>& list = hooks.side.Get(entry.slot).cells;
    list.clear();
    for (uint32_t i = 0; i < cells.Size(); ++i) { list.push_back(cells[i]); }
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    return Push(entry, true);
}

bool WeatherStage::QueueBiomeChange(uint32_t cell, Draoi::HexWorld::BiomeIndex biome)
{
    WeatherEntry entry;
    entry.kind = EntryKind::kBiomeChange;
    entry.cell = cell;
    entry.biome = biome;
    return Push(entry, false);
}

bool WeatherStage::QueuePreset(const WeatherPreset& preset)
{
    WeatherEntry entry;
    entry.kind = EntryKind::kPreset;
    entry.slot = hooks.side.Acquire();
    hooks.side.Get(entry.slot).preset = preset;
    return Push(entry, true);
}

bool WeatherStage::QueueStateReplace(const WeatherStateRecord& record)
{
    WeatherEntry entry;
    entry.kind = EntryKind::kStateReplace;
    entry.slot = hooks.side.Acquire();
    hooks.side.Get(entry.slot).record = record;
    return Push(entry, true);
}

bool WeatherStage::QueueClearWindOverrides()
{
    WeatherEntry entry;
    entry.kind = EntryKind::kClearWindOverrides;
    return Push(entry, false);
}

void WeatherStage::OnWorldComplete(uint32_t revision)
{
    if (IsFaulted())
    {
        DIA_LOG_ERROR(kLogChannel, "WTH: world %u announced but the stage has a parameter fault, not spinning up",
                      static_cast<unsigned>(revision));
        return;
    }
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
    DIA_LOG_INFO(kLogChannel, "WTH: world %u announced (%ux%u)", static_cast<unsigned>(revision),
                 static_cast<unsigned>(view.width), static_cast<unsigned>(view.height));
    hooks.SnapshotParameters(input->params);
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
