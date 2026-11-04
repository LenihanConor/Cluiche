#pragma once

#ifdef DIA_DEBUG

#include "Modules/TestStages/CellLayersStateBridgeModule.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaStreams/StreamWriter.h>

namespace CluicheTest {

// Sim-thread end of the cell-layer viewer state snapshot (see CellLayersStateBridgeModule for the stream list). A host stage that
// owns a CellLayerViewerDomain calls Attach(viewer) once the viewer exists and registers the returned adapter with the
// DebugDomain registry INSTEAD of the viewer (the registry is read on Main). Each Sim tick DoUpdate drains the demand events and,
// while armed, writes the viewer's GetJSONState (plus simFrame) to the CellViewerState FrameStream. Detach before the viewer dies.
class CellLayersStateRelayModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Sim end of the cell-layer viewer state snapshot: publishes the viewer state while demanded";

    explicit CellLayersStateRelayModule(const Dia::Core::StringCRC& instanceId);
    ~CellLayersStateRelayModule() override;

    // SimPU. Returns the adapter to register with the DebugDomain registry, or nullptr if the channel is not available. The relay
    // keeps `viewer` (for publishing) and binds the adapter's OnCommand to it; the adapter holds no other reference to it.
    Dia::VisualDebugger::IDebugDomain* Attach(Dia::VisualDebugger::IDebugDomain& viewer);
    // SimPU. Drops the adapter and publisher. The caller unregisters the adapter from the registry first.
    void Detach();
    // Same pair for the Cell History timeline domain (own snapshot/demand/channel streams). The adapter is for the DebugDomain registry.
    Dia::VisualDebugger::IDebugDomain* AttachHistory(Dia::VisualDebugger::IDebugDomain& domain);
    void DetachHistory();
    bool IsAttached() const { return static_cast<bool>(mAdapter); }
    uint32_t PublishCount() const { return mPublisher ? mPublisher->PublishCount() : 0u; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    class SnapshotSink : public Dia::CellLayers::IViewerStateSink
    {
    public:
        explicit SnapshotSink(Dia::ApplicationFlow::StreamWriter<Dia::CellLayers::ViewerStateSlot>& out)
            : mOut(out), mSlot(Dia::Core::MakeUnique<Dia::CellLayers::ViewerStateSlot>()) {}
        void Write(const char* json, uint32_t length) override;
    private:
        Dia::ApplicationFlow::StreamWriter<Dia::CellLayers::ViewerStateSlot>& mOut;
        Dia::Core::UniquePtr<Dia::CellLayers::ViewerStateSlot> mSlot;   // 32 KB: heap, not stack
    };

    Dia::ApplicationFlow::StreamWriter<Dia::CellLayers::ViewerStateSlot> mSnapshotOut{this, CellLayersStateBridgeModule::kSnapshotStreamId};
    Dia::ApplicationFlow::EventStreamReader<Dia::CellLayers::ViewerStateDemandEvent> mDemandIn{this, CellLayersStateBridgeModule::kDemandStreamId};
    Dia::ApplicationFlow::ServiceStreamReader<CellViewerStateChannel> mChannel{this, CellLayersStateBridgeModule::kChannelStreamId};
    SnapshotSink mSink{mSnapshotOut};
    Dia::Core::UniquePtr<Dia::CellLayers::ViewerStateSnapshotPublisher> mPublisher;
    Dia::Core::UniquePtr<Dia::CellLayers::ViewerStateAdapter> mAdapter;

    Dia::ApplicationFlow::StreamWriter<Dia::CellLayers::ViewerStateSlot> mHistorySnapshotOut{this, CellLayersStateBridgeModule::kHistorySnapshotStreamId};
    Dia::ApplicationFlow::EventStreamReader<Dia::CellLayers::ViewerStateDemandEvent> mHistoryDemandIn{this, CellLayersStateBridgeModule::kHistoryDemandStreamId};
    Dia::ApplicationFlow::ServiceStreamReader<CellViewerStateChannel> mHistoryChannel{this, CellLayersStateBridgeModule::kHistoryChannelStreamId};
    SnapshotSink mHistorySink{mHistorySnapshotOut};
    Dia::Core::UniquePtr<Dia::CellLayers::ViewerStateSnapshotPublisher> mHistoryPublisher;
    Dia::Core::UniquePtr<Dia::CellLayers::ViewerStateAdapter> mHistoryAdapter;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
