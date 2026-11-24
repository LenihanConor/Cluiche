#include "Modules/Weather/WeatherModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <Shared/Weather/WeatherTerrainBuilder.h>
#include <Shared/World/WorldSimNames.h>

namespace CoW {

// The production seams over WorldGenerationModule: the world read (copied into a WeatherTerrain, never retained) and
// the generation hold.
class WeatherModule::WorldGenAdapter : public Weather::IWeatherWorldSource, public WorldStep::IGenerationHold
{
public:
    explicit WorldGenAdapter(WorldGenerationModule& worldGen) : mWorldGen(worldGen) {}

    bool GetWorld(WorldStep::WorldView& view, Draoi::HexWorld::Weather::WeatherTerrain& terrain) override
    {
        const Draoi::HexWorld::WorldStructure* world = mWorldGen.GetCurrentWorld();
        // A newer world than the announced one exists: its own event follows, so skip this one.
        if (world == nullptr || mWorldGen.GetWorldRevision() != view.revision) { return false; }
        if (!Weather::BuildWeatherTerrain(*world, terrain)) { return false; }
        view.present = true;
        view.width = terrain.width;
        view.height = terrain.height;
        return true;
    }
    void Hold() override { mWorldGen.HoldGeneration(); }
    void Release() override { mWorldGen.ReleaseGeneration(); }

private:
    WorldGenerationModule& mWorldGen;
};

const Dia::Core::StringCRC WeatherModule::kTypeId("WeatherModule");

WeatherModule::WeatherModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
}

WeatherModule::~WeatherModule() {}

Dia::Core::StringCRC WeatherModule::StageId() const
{
    return Dia::Core::StringCRC(World::StageId(World::kWeather).c_str());
}

void WeatherModule::OnConfigure(const char*) {}   // Task 5: spinup_weeks and parameters path

Dia::ApplicationFlow::StartResult WeatherModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    DIA_TRACE_ZONE("WeatherModule::DoStart", Dia::Observation::Trace::Category::kNone);

    WorldStepModule* worldStep = nullptr;
    Dia::MessageBus::Bus* bus = nullptr;
    if (mUseTestHosts)
    {
        worldStep = mTestHosts.worldStep;
        bus = mTestHosts.bus;
        mStage.SetWorldSource(mTestHosts.worldSource);
        mStage.SetGenerationHold(mTestHosts.generationHold);
    }
    else
    {
        worldStep = mWorldStepRef.Get();
        Cluiche::AppFlow::MessageBusHostModule* busHost = mBusRef.Get();
        WorldGenerationModule* worldGen = mWorldGenRef.Get();
        if (busHost == nullptr || worldGen == nullptr)
            return StartResult::kLoading;
        bus = &busHost->GetBus();
        mWorldGen = worldGen;
        mAdapter = std::make_unique<WorldGenAdapter>(*worldGen);
        mStage.SetWorldSource(mAdapter.get());
        mStage.SetGenerationHold(mAdapter.get());
    }
    if (worldStep == nullptr)
        return StartResult::kLoading;

    mStage.Start();   // takes the generation hold
    if (!worldStep->RegisterStage(StageId(), mStage))
    {
        DIA_LOG_ERROR(Weather::kLogChannel, "WeatherModule: WorldStepModule refused stage id '%s'",
                      World::StageId(World::kWeather).c_str());
        mStage.Stop();
        return StartResult::kFailed;
    }
    mWorldStep = worldStep;
    mRegistered = true;

    if (bus != nullptr)
    {
        bus->RegisterType<WorldGenerationComplete, 4>();
        bus->RegisterType<WorldGenerationFailed, 4>();
        mSubscriptions[0] = bus->Subscribe<WorldGenerationComplete>(
            GetInstanceId(), [this](const WorldGenerationComplete& msg) { OnWorldComplete(msg); });
        mSubscriptions[1] = bus->Subscribe<WorldGenerationFailed>(
            GetInstanceId(), [this](const WorldGenerationFailed&) { OnWorldFailed(); });
        if (!mSubscriptions[0].IsValid() || !mSubscriptions[1].IsValid())
        {
            DIA_LOG_ERROR(Weather::kLogChannel, "WeatherModule: a bus subscription failed (handler pool full?)");
        }
    }
    mStage.NotifySubscribed();   // generation may now announce worlds: we are listening
    if (mWorldGen != nullptr && mWorldGen->GetWorldRevision() > 0)
    {
        // A world already exists (imported before we started): ingest it now.
        if (mWorldGen->AreBiomesLoaded()) { mStage.SetRegistry(&mWorldGen->GetBiomeRegistry()); }
        mStage.OnWorldComplete(mWorldGen->GetWorldRevision());
    }
    DIA_LOG_INFO(Weather::kLogChannel, "WeatherModule: stage '%s' registered", World::StageId(World::kWeather).c_str());
    return StartResult::kReady;
}

void WeatherModule::DoUpdate(const Dia::SimTime::SimTimeContext&)
{
    mStage.Update();
}

void WeatherModule::OnWorldComplete(const WorldGenerationComplete& msg)
{
    if (mWorldGen != nullptr && mWorldGen->AreBiomesLoaded()) { mStage.SetRegistry(&mWorldGen->GetBiomeRegistry()); }
    mStage.OnWorldComplete(msg.worldRevision);
}

void WeatherModule::OnWorldFailed()
{
    mStage.OnWorldFailed();
}

void WeatherModule::Unsubscribe()
{
    for (auto& handle : mSubscriptions)
        handle = Dia::MessageBus::BusSubscriptionHandle();
}

Dia::ApplicationFlow::StopResult WeatherModule::DoStop()
{
    DIA_TRACE_ZONE("WeatherModule::DoStop", Dia::Observation::Trace::Category::kNone);
    if (mRegistered && mWorldStep != nullptr)
        mWorldStep->UnregisterStage(StageId());
    mRegistered = false;
    mWorldStep = nullptr;
    Unsubscribe();
    mStage.Stop();
    mStage.SetWorldSource(nullptr);
    mStage.SetGenerationHold(nullptr);
    mWorldGen = nullptr;
    mAdapter.reset();
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using WeatherModule_ = CoW::WeatherModule; }
DIA_MODULE(WeatherModule_);
DIA_DESCRIBE(WeatherModule_::kTypeId, "A4 weather: the world-step weather stage host.");
