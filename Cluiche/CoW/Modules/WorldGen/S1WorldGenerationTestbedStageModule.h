#pragma once
#include "Modules/WorldGenerationModule.h"   // first: RandomStream.h must precede windows.h (min/max macros)
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/ServiceStreamReader.h>
#include "Modules/InputStreamModule.h"
#include "Types/AssetLoadStatus.h"
#ifdef DIA_DEBUG
#include "Modules/DomainStateRelayModule.h"
#include "Modules/VisualDebuggerModule.h"
#include "Modules/WorldGenDebugDomain.h"
#include "Modules/WorldGenViewHost.h"
#include "Modules/WorldGenMirrorRegistrar.h"
#include "Modules/Camera2DModule.h"
#include "Modules/SimQueryHostModule.h"
#include <DiaCellLayers/CellLayersModule.h>
#include <memory>
#endif

namespace Dia { namespace Graphics { class CellFieldRegistry; } }
namespace Dia { namespace ValueSchema { class ValueSchemaService; } }

namespace CoW {

// Placeholder game logic for the S1WorldGenerationTestbedStage stage (SimPU).
// Waits for the stage's assets, logs once it is running, then idles.
// Escape returns to the Boot menu.
class S1WorldGenerationTestbedStageModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "S1WorldGenerationTestbedStage stage logic (placeholder: logs and idles)";
    explicit S1WorldGenerationTestbedStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void NavigateTo(const Dia::Core::StringCRC& target);

    Dia::ApplicationFlow::ModuleRef<WorldGenerationModule> mWorldGen{this};
#ifdef DIA_DEBUG
    // Panel wiring (SimPU): the domain lives here; Main sees it only through the relay's snapshot adapter.
    // finalAttempt: build the card even when the map view cannot be hosted (services missing), warning once.
    bool TryBuildPanel(bool finalAttempt);
    void TeardownPanel();
    // Hosts the DiaCellLayers viewer over the generated world; false = services not ready (or build failed).
    bool TryBuildView(Cluiche::AppFlow::VisualDebuggerModule& vd);
    void TeardownView();
    // cow.worldgen.view: sim-hosted status query for the render E2E (CoW-only, works with or without the viewer).
    void RegisterViewQuery();
    void UnregisterViewQuery();
    bool mViewQueryRegistered = false;
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Camera2DModule> mCamera{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::SimQueryHostModule> mSimQueryHost{this};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Graphics::CellFieldRegistry> mCellFieldStream{this, "KernelCellFieldRegistry"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::ValueSchema::ValueSchemaService> mSchemaStream{this, "ValueSchemaService"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::CellLayers::CellLayerRegistry> mLayerRegistryStream{this, Dia::CellLayers::CellLayersModule::kServiceStreamId};
    std::unique_ptr<WorldGenMirrorRegistrar> mViewRegistrar;   // outlives mViewHost: Teardown unregisters the cells.* commands through it
    std::unique_ptr<WorldGenViewHost> mViewHost;
    bool mViewLayerRegistered = false;
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::DomainStateRelayModule> mStateRelay{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebugger{this};
    std::unique_ptr<WorldGenDebugDomain> mPanelDomainImpl;
    Dia::VisualDebugger::IDebugDomain* mPanelAdapter = nullptr;
    bool mPanelRegistered = false;
    bool mPanelWarned = false;
    bool mViewWarned = false;
    unsigned mPanelWaitFrames = 0;
#endif

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::InputStreamModule> mInput{this};

    // Stage asset load progress, published by AssetServiceModule (MainPU).
    Dia::ApplicationFlow::ServiceStreamReader<Cluiche::AppFlow::AssetLoadStatus> mAssetLoadStatus{this, "AssetLoadStatus"};
    // Asset lookups for stage content.
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::AssetRuntime> mAssetRuntime{this, "AssetRuntimeService"};
    // Navigation hold: AutomationModule holds every transition until released.
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService> mAutomation{this, "AutomationService"};

    bool mWaitLogged = false;
    bool mExitRequested = false;
};

} // namespace CoW
