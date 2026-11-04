#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaStreams/StreamReader.h>

namespace CluicheTest {

// Main-thread end of the cell-layer viewer state snapshot (Streams Migration Task 4). The viewer lives on SimPU; Main reaches its
// panel state through streams only:
//   CellViewerState        FrameStream  Sim -> Main  ViewerStateSlot (the latest snapshot JSON)         read here
//   CellViewerDemand       EventStream  Main -> Sim  ViewerStateDemandEvent (a state was requested)     written here
//   CellViewerStateChannel ServiceStream Main        CellViewerStateChannel (this module's channel)     provided here
// The Sim stage consumes the channel, builds a ViewerStateAdapter on it and registers that adapter (not the live viewer) with the
// DebugDomain registry. The adapter's GetJSONState runs on Main and only calls the channel: ReadLatest copies the newest slot,
// RequestState records demand, which DoUpdate forwards as at most one event per Main tick.
class CellViewerStateChannel
    : public Dia::CellLayers::IViewerStateSource
    , public Dia::CellLayers::IViewerStateDemand
{
public:
    // Dia::CellLayers::IViewerStateSource (Main thread)
    bool ReadLatest(char* buffer, uint32_t capacity, uint32_t& length) override;
    // Dia::CellLayers::IViewerStateDemand (Main thread)
    void RequestState() override { mDemandPending = true; }

    void SetSnapshotReader(Dia::ApplicationFlow::StreamReader<Dia::CellLayers::ViewerStateSlot>* reader) { mReader = reader; }
    bool TakeDemand() { const bool d = mDemandPending; mDemandPending = false; return d; }

private:
    Dia::ApplicationFlow::StreamReader<Dia::CellLayers::ViewerStateSlot>* mReader = nullptr;
    bool mDemandPending = false;   // Main thread only (GetJSONState and DoUpdate both run on MainPU)
};

class CellLayersStateBridgeModule : public Dia::ApplicationFlow::MainModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Main end of the cell-layer viewer state snapshot: reads the snapshot FrameStream, signals demand";
    static constexpr const char* kSnapshotStreamId = "CellViewerState";
    static constexpr const char* kDemandStreamId   = "CellViewerDemand";
    static constexpr const char* kChannelStreamId  = "CellViewerStateChannel";
    // Second, identical set for the Cell History timeline card domain ("cellhistory").
    static constexpr const char* kHistorySnapshotStreamId = "CellHistoryState";
    static constexpr const char* kHistoryDemandStreamId   = "CellHistoryDemand";
    static constexpr const char* kHistoryChannelStreamId  = "CellHistoryStateChannel";

    explicit CellLayersStateBridgeModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamReader<Dia::CellLayers::ViewerStateSlot> mSnapshotIn{this, kSnapshotStreamId};
    Dia::ApplicationFlow::EventStreamWriter<Dia::CellLayers::ViewerStateDemandEvent> mDemandOut{this, kDemandStreamId};
    Dia::ApplicationFlow::ServiceStreamWriter<CellViewerStateChannel> mChannelOut{this, kChannelStreamId};
    CellViewerStateChannel mChannel;

    Dia::ApplicationFlow::StreamReader<Dia::CellLayers::ViewerStateSlot> mHistorySnapshotIn{this, kHistorySnapshotStreamId};
    Dia::ApplicationFlow::EventStreamWriter<Dia::CellLayers::ViewerStateDemandEvent> mHistoryDemandOut{this, kHistoryDemandStreamId};
    Dia::ApplicationFlow::ServiceStreamWriter<CellViewerStateChannel> mHistoryChannelOut{this, kHistoryChannelStreamId};
    CellViewerStateChannel mHistoryChannel;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
