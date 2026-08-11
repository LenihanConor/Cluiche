#include "Modules/EconomyInspectorModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Streams/ServiceStreamStore.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaDebugServer/DebugServer.h>

#include "Modules/DebugServerHostModule.h"
#include "Modules/InspectorSources/EconomySchemaSource.h"
#include "Modules/InspectorSources/EconomyInstancesSource.h"
#include "Modules/InspectorSources/EconomyModifiersSource.h"
#include "Modules/InspectorSources/EconomyEventsSource.h"

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC EconomyInspectorModule::kTypeId("EconomyInspectorModule");

EconomyInspectorModule::EconomyInspectorModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

EconomyInspectorModule::~EconomyInspectorModule()
{
    DeactivateSources();
}

void EconomyInspectorModule::OnConnectStreams(Dia::ApplicationFlow::Application& /*app*/)
{
    // No streams needed — sources use direct DebugServer pointer
}

Dia::ApplicationFlow::StartResult EconomyInspectorModule::DoStart()
{
    DIA_LOG_INFO("Application", "EconomyInspectorModule::DoStart");

    auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(GetApplication());
    if (app)
    {
        auto* store = app->FindStream(
            Dia::Core::StringCRC(DebugServerHostModule::kServiceStreamId));
        auto* typed = dynamic_cast<
            Dia::ApplicationFlow::ServiceStreamStore<Dia::DebugServer::DebugServer*>*>(store);
        if (typed)
            mDebugServer = typed->Get();
        else
            DIA_LOG_WARNING("Application", "EconomyInspectorModule: DebugServerService stream not found");
    }

    if (!mDebugServer)
        DIA_LOG_WARNING("Application", "EconomyInspectorModule: no DebugServer — sources will not push");

    return Dia::ApplicationFlow::StartResult::kReady;
}

void EconomyInspectorModule::DoUpdate(float dt)
{
    DIA_TRACE_ZONE("EconomyInspectorModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    if (!mSourcesActive || !mDebugServer)
        return;

    const int connCount = mDebugServer->GetConnectionCount();
    const int subCount  = static_cast<int>(mDebugServer->GetStats().subscriptionCount);

    if (mSchemaSource)    mSchemaSource->Tick(dt, connCount, subCount);
    if (mInstancesSource) mInstancesSource->Tick(dt, connCount, subCount);
    if (mModifiersSource) mModifiersSource->Tick(dt, connCount, subCount);
    if (mEventsSource)    mEventsSource->Tick(dt, connCount, subCount);
}

Dia::ApplicationFlow::StopResult EconomyInspectorModule::DoStop()
{
    DIA_LOG_INFO("Application", "EconomyInspectorModule::DoStop");
    DeactivateSources();
    mDebugServer = nullptr;
    return Dia::ApplicationFlow::StopResult::kDone;
}

void EconomyInspectorModule::Bind(Dia::Economy::EconomySystem& system,
                                   const Dia::Economy::EconomySchema& schema)
{
    if (mSourcesActive)
        DeactivateSources();

    mSchemaSource    = std::make_unique<EconomySchemaSource>(schema, system);
    mInstancesSource = std::make_unique<EconomyInstancesSource>(system);
    mModifiersSource = std::make_unique<EconomyModifiersSource>(system);
    mEventsSource    = std::make_unique<EconomyEventsSource>(system);

    ActivateSources();
    DIA_LOG_INFO("Application", "EconomyInspectorModule::Bind — 4 sources activated");
}

void EconomyInspectorModule::Unbind()
{
    DeactivateSources();
    mSchemaSource.reset();
    mInstancesSource.reset();
    mModifiersSource.reset();
    mEventsSource.reset();
    DIA_LOG_INFO("Application", "EconomyInspectorModule::Unbind — sources released");
}

void EconomyInspectorModule::ActivateSources()
{
    if (!mDebugServer || mSourcesActive)
        return;
    if (mSchemaSource)    mSchemaSource->Activate(mDebugServer);
    if (mInstancesSource) mInstancesSource->Activate(mDebugServer);
    if (mModifiersSource) mModifiersSource->Activate(mDebugServer);
    if (mEventsSource)    mEventsSource->Activate(mDebugServer);
    mSourcesActive = true;
}

void EconomyInspectorModule::DeactivateSources()
{
    if (!mSourcesActive)
        return;
    if (mEventsSource)    mEventsSource->Deactivate();
    if (mModifiersSource) mModifiersSource->Deactivate();
    if (mInstancesSource) mInstancesSource->Deactivate();
    if (mSchemaSource)    mSchemaSource->Deactivate();
    mSourcesActive = false;
}

} } // namespace Cluiche::AppFlow

namespace { using EconomyInspectorModule_ = Cluiche::AppFlow::EconomyInspectorModule; }
DIA_MODULE(EconomyInspectorModule_);
DIA_DESCRIBE(EconomyInspectorModule_::kTypeId,
    "Pushes economy inspector payloads (instances/modifiers/events/schema) to the debug server");

#endif // DIA_DEBUG
