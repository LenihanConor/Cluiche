#pragma once
// A4 WeatherModule: the SimModule that hosts the weather stage and registers it with WorldStepModule.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-module.md. Scaffold (plan Task 1): lifecycle and
// registration only; world ingest, parameters, subscriptions and health arrive in Tasks 2-5.
#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaMessageBus/Bus.h>
#include <DiaMessageBus/BusSubscriptionHandle.h>
#include <DiaCore/CRC/StringCRC.h>
#include <Modules/GameSpeed/GameSpeedMessages.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/WorldStep/WorldStepModule.h>
#include <Shared/Weather/WeatherStage.h>

#include <memory>
#include <vector>

namespace CoW {

class WeatherModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "A4 weather: the world-step weather stage host";

    explicit WeatherModule(const Dia::Core::StringCRC& instanceId);
    ~WeatherModule() override;

    Weather::WeatherStage& GetStage() { return mStage; }
    const Weather::WeatherStage& GetStage() const { return mStage; }

    // Parameters: the sidecar is read through this source (borrowed; null = the stage keeps the parameters it was handed).
    // Task 5 supplies the production file source from config. Packs are applied after each load and reload.
    void SetSidecarSource(Weather::IWeatherSidecarSource* source) { mSidecarSource = source; }
    void SetActivePacks(const std::vector<Json::Value>& packs) { mPacks = packs; }
    void SetSpinupWeeks(uint32_t weeks) { mStage.SetSpinupWeeks(weeks); }

    // Queue API (SimPU, any time). Thin over the stage; see WeatherStage for the contracts.
    bool QueueParameterSet(const char* dottedKey, float value, Weather::ParamError& err) { return mStage.QueueParameterSet(dottedKey, value, err); }
    bool QueueParameterOverrides(const Json::Value& pack, Weather::ParamError& err) { return mStage.QueueParameterOverrides(pack, err); }
    bool QueueReload() { return mStage.QueueReload(); }
    bool QueueForcing(const Draoi::HexWorld::Weather::WeatherForcing& f) { return mStage.QueueForcing(f); }
    bool QueueRegionForcing(const Draoi::HexWorld::Weather::WeatherForcing& f, const Dia::Core::Containers::DynamicArray<uint32_t>& cells) { return mStage.QueueRegionForcing(f, cells); }
    bool QueueBiomeChange(uint32_t cell, Draoi::HexWorld::BiomeIndex biome) { return mStage.QueueBiomeChange(cell, biome); }
    bool QueuePreset(const Weather::WeatherPreset& preset) { return mStage.QueuePreset(preset); }
    bool QueueStateReplace(const Weather::WeatherStateRecord& record) { return mStage.QueueStateReplace(record); }
    bool QueueClearWindOverrides() { return mStage.QueueClearWindOverrides(); }

    // Test seam: injects collaborators so the module runs without a ProcessingUnit (no ModuleRef resolution).
    struct TestHosts
    {
        WorldStepModule* worldStep = nullptr;
        Dia::MessageBus::Bus* bus = nullptr;                    // null: no subscriptions
        Weather::IWeatherWorldSource* worldSource = nullptr;    // hand-built terrain instead of WorldGenerationModule
        WorldStep::IGenerationHold* generationHold = nullptr;
    };
    void UseTestHosts(const TestHosts& hosts) { mTestHosts = hosts; mUseTestHosts = true; }

protected:
    void OnConfigure(const char* configJson) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    class WorldGenAdapter;   // IWeatherWorldSource + IGenerationHold over WorldGenerationModule

    Dia::Core::StringCRC StageId() const;
    void OnWorldComplete(const WorldGenerationComplete& msg);
    void OnWorldFailed();
    void Unsubscribe();
    void TryLoadParameters();   // once per start, as soon as the stage has a biome registry

    std::unique_ptr<WorldGenAdapter> mAdapter;
    Dia::MessageBus::BusSubscriptionHandle mSubscriptions[2];
    WorldGenerationModule* mWorldGen = nullptr;   // borrowed; null in test-hosts mode

    Weather::IWeatherSidecarSource* mSidecarSource = nullptr;   // borrowed
    std::vector<Json::Value> mPacks;
    bool mParamsAttempted = false;

    Weather::WeatherStage mStage;
    WorldStepModule* mWorldStep = nullptr;   // borrowed; set while registered
    bool mRegistered = false;
    TestHosts mTestHosts;
    bool mUseTestHosts = false;

    Dia::ApplicationFlow::ModuleRef<WorldStepModule> mWorldStepRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBusRef{this};
    Dia::ApplicationFlow::ModuleRef<WorldGenerationModule> mWorldGenRef{this};
};

} // namespace CoW
