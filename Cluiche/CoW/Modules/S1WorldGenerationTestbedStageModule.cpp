#include "Modules/S1WorldGenerationTestbedStageModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaInput/EKey.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {

const Dia::Core::StringCRC S1WorldGenerationTestbedStageModule::kTypeId("S1WorldGenerationTestbedStageModule");

S1WorldGenerationTestbedStageModule::S1WorldGenerationTestbedStageModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult S1WorldGenerationTestbedStageModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    using LoadState = Cluiche::AppFlow::AssetLoadStatus::State;

    if (!mWaitLogged)
    {
        DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: waiting for stage assets");
        mWaitLogged = true;
    }

    if (!mAssetRuntime.IsAvailable() || !mAssetLoadStatus.IsAvailable())
        return StartResult::kLoading;

    const Cluiche::AppFlow::AssetLoadStatus& status = mAssetLoadStatus.Get();
    if (status.stageId != Dia::Core::StringCRC("S1WorldGenerationTestbedStage"))
        return StartResult::kLoading;

    if (status.state == LoadState::kFailed)
    {
        DIA_LOG_ERROR("CoW", "S1WorldGenerationTestbedStageModule: stage asset load failed");
        return StartResult::kFailed;
    }
    if (status.state != LoadState::kComplete)
        return StartResult::kLoading;

#ifdef DIA_DEBUG
    // Wait (bounded) for the relay and visual debugger; without them the stage still works headless.
    if (!mPanelAdapter && !TryBuildPanel(mPanelWaitFrames >= 299))
    {
        if (++mPanelWaitFrames < 300)
            return StartResult::kLoading;
        if (!mPanelWarned)
        {
            DIA_LOG_WARNING("CoW", "S1WorldGenerationTestbedStageModule: state relay / visual debugger unavailable - World Generation card not shown");
            mPanelWarned = true;
        }
    }
#endif

    mExitRequested = false;
    DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: started (S1WorldGenerationTestbedStage stage running, %u assets) - press Escape to return to Boot",
                 status.loaded);
    return StartResult::kReady;
}

void S1WorldGenerationTestbedStageModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
#ifdef DIA_DEBUG
    if (mViewHost)
        mViewHost->Update(ctx);   // size check, input -> camera -> pointer frame (the viewer draws from the debug layer)
#else
    (void)ctx;
#endif

    if (mExitRequested)
        return;

    if (Cluiche::AppFlow::InputStreamModule* input = mInput.Get())
    {
        if (input->WasKeyPressed(Dia::Input::EKey::Escape))
        {
            DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: Escape pressed - returning to Boot");
            mExitRequested = true;
            NavigateTo(Dia::Core::StringCRC("Boot"));
        }
    }
}

Dia::ApplicationFlow::StopResult S1WorldGenerationTestbedStageModule::DoStop()
{
    DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: stopped");
    mWaitLogged = false;
#ifdef DIA_DEBUG
    TeardownPanel();
#endif
    return Dia::ApplicationFlow::StopResult::kDone;
}

#ifdef DIA_DEBUG
bool S1WorldGenerationTestbedStageModule::TryBuildPanel(bool finalAttempt)
{
    WorldGenerationModule* wg = mWorldGen.Get();
    Cluiche::AppFlow::DomainStateRelayModule* relay = mStateRelay.Get();
    Cluiche::AppFlow::VisualDebuggerModule* vd = mVisualDebugger.Get();
    if (wg == nullptr || relay == nullptr || vd == nullptr)
        return false;

    // The map view needs services that publish a few frames after start; wait for them unless this is the last attempt.
    const bool viewReady = Dia::CellLayers::FindCellLayerRegistry(mLayerRegistryStream) != nullptr &&
                           mCellFieldStream.IsAvailable() && mCamera.Get() != nullptr;
    if (!viewReady && !finalAttempt)
        return false;

    auto domain = std::make_unique<WorldGenDebugDomain>(*wg);
    Dia::VisualDebugger::IDebugDomain* adapter = relay->Attach(*domain);
    if (adapter == nullptr)
    {
        DIA_LOG_WARNING("CoW", "S1WorldGenerationTestbedStageModule: DomainStateRelay has no free slot - World Generation card not shown");
        mPanelWarned = true;
        return false;
    }
    mPanelDomainImpl = std::move(domain);
    mPanelAdapter = adapter;

    if (viewReady && TryBuildView(*vd))
    {
        mPanelDomainImpl->SetView(mViewHost.get());
    }
    else if (!mViewWarned)
    {
        DIA_LOG_WARNING("CoW", "S1WorldGenerationTestbedStageModule: map view unavailable (CellLayerRegistry / CellFieldRegistry / Camera2D) - card works without the viewer");
        mViewWarned = true;
    }

    vd->RegisterDomain(*adapter);
    mPanelRegistered = true;
    DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: World Generation card registered with the visual debugger");
    return true;
}

bool S1WorldGenerationTestbedStageModule::TryBuildView(Cluiche::AppFlow::VisualDebuggerModule& vd)
{
    WorldGenerationModule* wg = mWorldGen.Get();
    WorldGenViewHost::Deps deps;
    deps.registry  = Dia::CellLayers::FindCellLayerRegistry(mLayerRegistryStream);
    deps.cellField = mCellFieldStream.IsAvailable() ? &mCellFieldStream.Get() : nullptr;
    deps.schema    = mSchemaStream.IsAvailable() ? &mSchemaStream.Get() : nullptr;
    if (Cluiche::AppFlow::SimQueryHostModule* host = mSimQueryHost.Get())
    {
        mViewRegistrar = std::make_unique<WorldGenMirrorRegistrar>(*host);
        deps.registrar = mViewRegistrar.get();
    }
    deps.camera = mCamera.Get();
    deps.input  = mInput.Get();
    if (deps.schema == nullptr)
        DIA_LOG_WARNING("CoW", "S1WorldGenerationTestbedStageModule: no ValueSchemaService - map layers show without schema data");

    mViewHost = std::make_unique<WorldGenViewHost>(wg->GetSession(), deps);
    if (!mViewHost->Build())
    {
        mViewHost.reset();
        mViewRegistrar.reset();
        return false;
    }
    vd.GetLayerManager().Register(&mViewHost->DebugLayer(), 10, Dia::Core::StringCRC("S1WorldGenerationTestbedStage"));
    mViewLayerRegistered = true;
    return true;
}

void S1WorldGenerationTestbedStageModule::TeardownView()
{
    if (mViewLayerRegistered)
    {
        if (auto* vd = mVisualDebugger.Get())
            vd->GetLayerManager().Unregister(mViewHost->DebugLayer().GetLayerName());
        mViewLayerRegistered = false;
    }
    if (mPanelDomainImpl)
        mPanelDomainImpl->SetView(nullptr);
    if (mViewHost)
        mViewHost->Teardown();   // viewer + uploader + cells.* commands, before the relay detaches the domain
    mViewHost.reset();
    mViewRegistrar.reset();      // after the host: Teardown unregisters through it
}

void S1WorldGenerationTestbedStageModule::TeardownPanel()
{
    TeardownView();
    if (mPanelRegistered && mPanelAdapter != nullptr)
    {
        if (auto* vd = mVisualDebugger.Get())
            vd->UnregisterDomain(*mPanelAdapter);
    }
    if (mPanelAdapter != nullptr && mPanelDomainImpl)
    {
        if (auto* relay = mStateRelay.Get())
            relay->Detach(*mPanelDomainImpl);   // before the domain is destroyed
    }
    mPanelRegistered = false;
    mPanelAdapter = nullptr;
    mPanelDomainImpl.reset();
    mPanelWaitFrames = 0;
    mPanelWarned = false;
    mViewWarned = false;
}
#endif

void S1WorldGenerationTestbedStageModule::NavigateTo(const Dia::Core::StringCRC& target)
{
    // Same rule as SimNavigationHandlerModule: while AutomationModule holds
    // navigation, a plain TransitionTo() would wait on the guard forever.
    if (mAutomation.IsAvailable() && mAutomation.Get().IsHolding())
        mAutomation.Get().ReleaseNavigationHold(target);
    else
        TransitionTo(target);
}

void S1WorldGenerationTestbedStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mAssetLoadStatus.Connect(app);
    mAssetRuntime.Connect(app);
    mAutomation.Connect(app);
#ifdef DIA_DEBUG
    mCellFieldStream.Connect(app);
    mSchemaStream.Connect(app);
    mLayerRegistryStream.Connect(app);
#endif
}

} // namespace CoW

namespace { using S1WorldGenerationTestbedStageModule_ = CoW::S1WorldGenerationTestbedStageModule; }
DIA_MODULE(S1WorldGenerationTestbedStageModule_);
DIA_DESCRIBE(S1WorldGenerationTestbedStageModule_::kTypeId, "Placeholder logic for the S1WorldGenerationTestbedStage stage: logs on start, idles, Escape returns to Boot.");
