#pragma once

#ifdef DIA_DEBUG

#include "Modules/DomainStateBridgeModule.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaStreams/StreamWriter.h>

#include <memory>

namespace Cluiche::AppFlow {

// Sim-thread end of the pooled debug-domain state snapshot (see DomainStateBridgeModule). A host stage that owns a debug domain
// calls Attach(domain) once the domain exists and registers the returned adapter with the DebugDomain registry INSTEAD of the
// domain (the registry is read on Main). Each Sim tick DoUpdate drains demand events per slot and, while armed, publishes the
// domain's GetJSONState (plus simFrame) to that slot's FrameStream. Detach before the domain dies.
class DomainStateRelayModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Sim end of the pooled debug-domain state snapshot: publishes domain state while demanded";
    static constexpr uint32_t kSlotCount = DomainStateBridgeModule::kSlotCount;

    explicit DomainStateRelayModule(const Dia::Core::StringCRC& instanceId);
    ~DomainStateRelayModule() override;

    // SimPU. Takes the first free slot (or returns the existing adapter if `domain` is already attached). Returns the adapter to
    // register with the DebugDomain registry, or nullptr if the slot's channel is unavailable or no slot is free.
    Dia::VisualDebugger::IDebugDomain* Attach(Dia::VisualDebugger::IDebugDomain& domain);
    // SimPU. Releases the slot `domain` occupies (no-op if not attached). The caller unregisters the adapter from the registry first.
    void Detach(Dia::VisualDebugger::IDebugDomain& domain);
    bool IsAttached(const Dia::VisualDebugger::IDebugDomain& domain) const { return FindSlot(&domain) != kNoSlot; }
    uint32_t AttachedCount() const;
    uint32_t PublishCount(const Dia::VisualDebugger::IDebugDomain& domain) const;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    static constexpr uint32_t kNoSlot = 0xFFFFFFFFu;

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

    struct Slot
    {
        Slot(Dia::ApplicationFlow::Module* owner, uint32_t index);
        Dia::ApplicationFlow::StreamWriter<Dia::CellLayers::ViewerStateSlot> snapshotOut;
        Dia::ApplicationFlow::EventStreamReader<Dia::CellLayers::ViewerStateDemandEvent> demandIn;
        Dia::ApplicationFlow::ServiceStreamReader<DomainStateChannel> channel;
        SnapshotSink sink;
        Dia::VisualDebugger::IDebugDomain* domain = nullptr;
        Dia::Core::UniquePtr<Dia::CellLayers::ViewerStateSnapshotPublisher> publisher;
        Dia::Core::UniquePtr<Dia::CellLayers::ViewerStateAdapter> adapter;
    };

    uint32_t FindSlot(const Dia::VisualDebugger::IDebugDomain* domain) const;
    void ReleaseSlot(Slot& slot);

    std::unique_ptr<Slot> mSlots[kSlotCount];
};

} // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
