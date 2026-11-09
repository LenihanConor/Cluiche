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

#include <atomic>
#include <memory>

namespace Cluiche::AppFlow {

// Main-side channel of one pooled debug-domain snapshot slot. Same behaviour as the CluicheTest CellViewerStateChannel: ReadLatest
// copies the newest snapshot, RequestState records demand which DoUpdate forwards as at most one event per Main tick.
class DomainStateChannel
    : public Dia::CellLayers::IViewerStateSource
    , public Dia::CellLayers::IViewerStateDemand
{
public:
    bool ReadLatest(char* buffer, uint32_t capacity, uint32_t& length) override;
    void RequestState() override { mDemandPending.store(true, std::memory_order_relaxed); }

    void SetSnapshotReader(Dia::ApplicationFlow::StreamReader<Dia::CellLayers::ViewerStateSlot>* reader) { mReader = reader; }
    bool TakeDemand() { return mDemandPending.exchange(false, std::memory_order_relaxed); }

private:
    Dia::ApplicationFlow::StreamReader<Dia::CellLayers::ViewerStateSlot>* mReader = nullptr;
    // Written by RequestState and cleared by TakeDemand on DIFFERENT threads: the Sim-side snapshot publisher packs the registered
    // ViewerStateAdapter (S1 stage: VisualDebuggerModule::RegisterDomain(adapter)), so GetJSONState -> RequestState runs on the Sim
    // thread, while the bridge's DoUpdate (TakeDemand) runs on Main. Hence atomic; no ordering with other data is needed (the
    // flag only triggers an event that carries nothing).
    std::atomic<bool> mDemandPending{ false };
};

// Main-thread end of a fixed pool of kSlotCount generic debug-domain snapshot channels. Slot N uses streams
//   DomainStateSnapshotN (FrameStream Sim->Main), DomainStateDemandN (EventStream Main->Sim), DomainStateChannelN (ServiceStream Main).
// The Sim-side DomainStateRelayModule hands each Attach()ed domain the first free slot; Main and Sim agree through the slot index only.
class DomainStateBridgeModule : public Dia::ApplicationFlow::MainModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Main end of the pooled debug-domain state snapshot: reads snapshot FrameStreams, signals demand";
    static constexpr uint32_t kSlotCount = 4;
    static constexpr const char* kSnapshotStreamIds[kSlotCount] = { "DomainStateSnapshot0", "DomainStateSnapshot1", "DomainStateSnapshot2", "DomainStateSnapshot3" };
    static constexpr const char* kDemandStreamIds[kSlotCount]   = { "DomainStateDemand0", "DomainStateDemand1", "DomainStateDemand2", "DomainStateDemand3" };
    static constexpr const char* kChannelStreamIds[kSlotCount]  = { "DomainStateChannel0", "DomainStateChannel1", "DomainStateChannel2", "DomainStateChannel3" };

    explicit DomainStateBridgeModule(const Dia::Core::StringCRC& instanceId);
    ~DomainStateBridgeModule() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    struct Slot
    {
        Slot(Dia::ApplicationFlow::Module* owner, uint32_t index);
        Dia::ApplicationFlow::StreamReader<Dia::CellLayers::ViewerStateSlot> snapshotIn;
        Dia::ApplicationFlow::EventStreamWriter<Dia::CellLayers::ViewerStateDemandEvent> demandOut;
        Dia::ApplicationFlow::ServiceStreamWriter<DomainStateChannel> channelOut;
        DomainStateChannel channel;
    };
    std::unique_ptr<Slot> mSlots[kSlotCount];
};

} // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
