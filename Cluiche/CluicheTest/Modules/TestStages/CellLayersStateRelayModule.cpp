#include "Modules/TestStages/CellLayersStateRelayModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstring>

namespace CluicheTest {

const Dia::Core::StringCRC CellLayersStateRelayModule::kTypeId("CellLayersStateRelayModule");

CellLayersStateRelayModule::CellLayersStateRelayModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

CellLayersStateRelayModule::~CellLayersStateRelayModule() = default;

void CellLayersStateRelayModule::SnapshotSink::Write(const char* json, uint32_t length)
{
    Dia::CellLayers::ViewerStateSlot& slot = *mSlot;
    slot.length = length <= Dia::CellLayers::ViewerStateSlot::kCapacity ? length : Dia::CellLayers::ViewerStateSlot::kCapacity;
    memcpy(slot.json, json, slot.length);
    mOut.Write(slot, Dia::Core::TimeAbsolute::Zero());
}

void CellLayersStateRelayModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mSnapshotOut.Connect(app);
    mDemandIn.Connect(app);
    mChannel.Connect(app);
    mHistorySnapshotOut.Connect(app);
    mHistoryDemandIn.Connect(app);
    mHistoryChannel.Connect(app);
}

Dia::ApplicationFlow::StartResult CellLayersStateRelayModule::DoStart()
{
    // Wait for the Main-side bridge to publish its channel; hosts that depend on this module then find it ready.
    return (mChannel.IsAvailable() && mHistoryChannel.IsAvailable()) ?Dia::ApplicationFlow::StartResult::kReady : Dia::ApplicationFlow::StartResult::kLoading;
}

Dia::VisualDebugger::IDebugDomain* CellLayersStateRelayModule::Attach(Dia::VisualDebugger::IDebugDomain& viewer)
{
    if (!mChannel.IsAvailable())
    {
        DIA_LOG_WARNING("CEL", "CellLayersStateRelayModule: viewer state channel unavailable - panel state not served");
        return nullptr;
    }
    CellViewerStateChannel& channel = mChannel.Get();
    mPublisher = Dia::Core::MakeUnique<Dia::CellLayers::ViewerStateSnapshotPublisher>(viewer, mSink);
    mAdapter = Dia::Core::MakeUnique<Dia::CellLayers::ViewerStateAdapter>(viewer, channel, channel,
        Dia::CellLayers::ViewerStateAdapter::CommandFn(fastdelegate::MakeDelegate(&viewer, &Dia::VisualDebugger::IDebugDomain::OnCommand)));
    Dia::Observation::Health::HealthRegistry::Instance().Register(&mAdapter->Health());
    DIA_LOG_INFO("CEL", "CellLayersStateRelayModule: attached viewer '%s'; publishing on demand", viewer.GetDisplayName());
    return mAdapter.Get();
}

void CellLayersStateRelayModule::Detach()
{
    if (mAdapter)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mAdapter->Health());
        DIA_LOG_INFO("CEL", "CellLayersStateRelayModule: detached (%u snapshots published)", mPublisher ? mPublisher->PublishCount() : 0u);
    }
    mAdapter.Reset();
    mPublisher.Reset();
}

Dia::VisualDebugger::IDebugDomain* CellLayersStateRelayModule::AttachHistory(Dia::VisualDebugger::IDebugDomain& domain)
{
    if (!mHistoryChannel.IsAvailable())
    {
        DIA_LOG_WARNING("CEL", "CellLayersStateRelayModule: history state channel unavailable - timeline panel state not served");
        return nullptr;
    }
    CellViewerStateChannel& channel = mHistoryChannel.Get();
    mHistoryPublisher = Dia::Core::MakeUnique<Dia::CellLayers::ViewerStateSnapshotPublisher>(domain, mHistorySink);
    mHistoryAdapter = Dia::Core::MakeUnique<Dia::CellLayers::ViewerStateAdapter>(domain, channel, channel,
        Dia::CellLayers::ViewerStateAdapter::CommandFn(fastdelegate::MakeDelegate(&domain, &Dia::VisualDebugger::IDebugDomain::OnCommand)));
    DIA_LOG_INFO("CEL", "CellLayersStateRelayModule: attached history domain '%s'; publishing on demand", domain.GetDisplayName());
    return mHistoryAdapter.Get();
}

void CellLayersStateRelayModule::DetachHistory()
{
    mHistoryAdapter.Reset();
    mHistoryPublisher.Reset();
}

void CellLayersStateRelayModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    if (mHistoryPublisher)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::CellLayers::ViewerStateDemandEvent>, 8> historyDemand;
        mHistoryDemandIn.Consume(historyDemand);
        if (historyDemand.Size() > 0)
            mHistoryPublisher->Arm(ctx.tick);
        mHistoryPublisher->Update(ctx.tick);
    }
    if (!mPublisher)
        return;
    // Any demand event since the last tick (the Main-side adapter was asked for the viewer state) arms the publisher.
    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::CellLayers::ViewerStateDemandEvent>, 8> demand;
    mDemandIn.Consume(demand);
    if (demand.Size() > 0)
        mPublisher->Arm(ctx.tick);
    mPublisher->Update(ctx.tick);
}

Dia::ApplicationFlow::StopResult CellLayersStateRelayModule::DoStop()
{
    DetachHistory();
    Detach();
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CluicheTest

namespace { using CellLayersStateRelayModule_ = CluicheTest::CellLayersStateRelayModule; }
DIA_MODULE(CellLayersStateRelayModule_);
DIA_DESCRIBE(CellLayersStateRelayModule_::kTypeId, "Sim end of the cell-layer viewer state snapshot: publishes the viewer state while demanded.");

#endif // DIA_DEBUG
