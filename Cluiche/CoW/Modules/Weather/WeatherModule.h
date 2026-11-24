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

    std::unique_ptr<WorldGenAdapter> mAdapter;
    Dia::MessageBus::BusSubscriptionHandle mSubscriptions[2];
    WorldGenerationModule* mWorldGen = nullptr;   // borrowed; null in test-hosts mode

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
