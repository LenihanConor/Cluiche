#include "Modules/DomainStateBridgeModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstring>

namespace Cluiche::AppFlow {

const Dia::Core::StringCRC DomainStateBridgeModule::kTypeId("DomainStateBridgeModule");

bool DomainStateChannel::ReadLatest(char* buffer, uint32_t capacity, uint32_t& length)
{
    const Dia::CellLayers::ViewerStateSlot* slot = mReader != nullptr ? mReader->FetchLatest() : nullptr;
    if (slot == nullptr || buffer == nullptr)
        return false;
    uint32_t n = slot->length <= Dia::CellLayers::ViewerStateSlot::kCapacity ? slot->length : Dia::CellLayers::ViewerStateSlot::kCapacity;
    if (n > capacity)
        n = capacity;
    std::memcpy(buffer, slot->json, n);
    length = n;
    return true;
}

DomainStateBridgeModule::Slot::Slot(Dia::ApplicationFlow::Module* owner, uint32_t index)
    : snapshotIn(owner, kSnapshotStreamIds[index])
    , demandOut(owner, kDemandStreamIds[index])
    , channelOut(owner, kChannelStreamIds[index])
{
    channel.SetSnapshotReader(&snapshotIn);
}

DomainStateBridgeModule::DomainStateBridgeModule(const Dia::Core::StringCRC& instanceId)
    : MainModule(instanceId)
{
    for (uint32_t i = 0; i < kSlotCount; ++i)
        mSlots[i].reset(new Slot(this, i));
}

DomainStateBridgeModule::~DomainStateBridgeModule() = default;

void DomainStateBridgeModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    for (auto& s : mSlots)
    {
        s->snapshotIn.Connect(app);
        s->demandOut.Connect(app);
        s->channelOut.Connect(app);
    }
}

Dia::ApplicationFlow::StartResult DomainStateBridgeModule::DoStart()
{
    for (auto& s : mSlots)
        s->channelOut.Register(s->channel);
    DIA_LOG_INFO("DST", "DomainStateBridgeModule: %u domain state channels published", kSlotCount);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void DomainStateBridgeModule::DoUpdate(const Dia::SimTime::MainTimeContext& /*ctx*/)
{
    // One demand event per slot per Main tick, however many GetJSONState calls were made.
    for (auto& s : mSlots)
        if (s->channel.TakeDemand())
            s->demandOut.Send(Dia::CellLayers::ViewerStateDemandEvent{});
}

Dia::ApplicationFlow::StopResult DomainStateBridgeModule::DoStop()
{
    for (auto& s : mSlots)
        s->channelOut.Deregister();
    DIA_LOG_INFO("DST", "DomainStateBridgeModule: domain state channels withdrawn");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace Cluiche::AppFlow

namespace { using DomainStateBridgeModule_ = Cluiche::AppFlow::DomainStateBridgeModule; }
DIA_MODULE(DomainStateBridgeModule_);
DIA_DESCRIBE(DomainStateBridgeModule_::kTypeId, "Main end of the pooled debug-domain state snapshot: reads snapshot FrameStreams, signals demand.");

#endif // DIA_DEBUG
