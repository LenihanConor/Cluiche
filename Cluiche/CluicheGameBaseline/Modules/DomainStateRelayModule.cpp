#include "Modules/DomainStateRelayModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstring>

namespace Cluiche::AppFlow {

const Dia::Core::StringCRC DomainStateRelayModule::kTypeId("DomainStateRelayModule");

DomainStateRelayModule::Slot::Slot(Dia::ApplicationFlow::Module* owner, uint32_t index)
    : snapshotOut(owner, DomainStateBridgeModule::kSnapshotStreamIds[index])
    , demandIn(owner, DomainStateBridgeModule::kDemandStreamIds[index])
    , channel(owner, DomainStateBridgeModule::kChannelStreamIds[index])
    , sink(snapshotOut)
{}

DomainStateRelayModule::DomainStateRelayModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
    for (uint32_t i = 0; i < kSlotCount; ++i)
        mSlots[i].reset(new Slot(this, i));
}

DomainStateRelayModule::~DomainStateRelayModule() = default;

void DomainStateRelayModule::SnapshotSink::Write(const char* json, uint32_t length)
{
    Dia::CellLayers::ViewerStateSlot& slot = *mSlot;
    slot.length = length <= Dia::CellLayers::ViewerStateSlot::kCapacity ? length : Dia::CellLayers::ViewerStateSlot::kCapacity;
    memcpy(slot.json, json, slot.length);
    mOut.Write(slot, Dia::Core::TimeAbsolute::Zero());
}

void DomainStateRelayModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    for (auto& s : mSlots)
    {
        s->snapshotOut.Connect(app);
        s->demandIn.Connect(app);
        s->channel.Connect(app);
    }
}

Dia::ApplicationFlow::StartResult DomainStateRelayModule::DoStart()
{
    // Wait for the Main-side bridge to publish all channels.
    for (auto& s : mSlots)
        if (!s->channel.IsAvailable())
            return Dia::ApplicationFlow::StartResult::kLoading;
    return Dia::ApplicationFlow::StartResult::kReady;
}

uint32_t DomainStateRelayModule::FindSlot(const Dia::VisualDebugger::IDebugDomain* domain) const
{
    for (uint32_t i = 0; i < kSlotCount; ++i)
        if (mSlots[i]->domain == domain && mSlots[i]->adapter)
            return i;
    return kNoSlot;
}

Dia::VisualDebugger::IDebugDomain* DomainStateRelayModule::Attach(Dia::VisualDebugger::IDebugDomain& domain)
{
    const uint32_t existing = FindSlot(&domain);
    if (existing != kNoSlot)
        return mSlots[existing]->adapter.Get();

    for (uint32_t i = 0; i < kSlotCount; ++i)
    {
        Slot& slot = *mSlots[i];
        if (slot.adapter)
            continue;
        if (!slot.channel.IsAvailable())
        {
            DIA_LOG_WARNING("DST", "DomainStateRelayModule: channel %u unavailable - state of '%s' not served", i, domain.GetDisplayName());
            return nullptr;
        }
        DomainStateChannel& channel = slot.channel.Get();
        slot.domain = &domain;
        slot.publisher = Dia::Core::MakeUnique<Dia::CellLayers::ViewerStateSnapshotPublisher>(domain, slot.sink);
        slot.adapter = Dia::Core::MakeUnique<Dia::CellLayers::ViewerStateAdapter>(domain, channel, channel,
            Dia::CellLayers::ViewerStateAdapter::CommandFn(fastdelegate::MakeDelegate(&domain, &Dia::VisualDebugger::IDebugDomain::OnCommand)));
        Dia::Observation::Health::HealthRegistry::Instance().Register(&slot.adapter->Health());
        DIA_LOG_INFO("DST", "DomainStateRelayModule: attached '%s' in slot %u; publishing on demand", domain.GetDisplayName(), i);
        return slot.adapter.Get();
    }
    DIA_LOG_WARNING("DST", "DomainStateRelayModule: no free slot (%u in use) - state of '%s' not served", kSlotCount, domain.GetDisplayName());
    return nullptr;
}

void DomainStateRelayModule::ReleaseSlot(Slot& slot)
{
    if (slot.adapter)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&slot.adapter->Health());
        DIA_LOG_INFO("DST", "DomainStateRelayModule: detached (%u snapshots published)", slot.publisher ? slot.publisher->PublishCount() : 0u);
    }
    slot.adapter.Reset();
    slot.publisher.Reset();
    slot.domain = nullptr;
}

void DomainStateRelayModule::Detach(Dia::VisualDebugger::IDebugDomain& domain)
{
    const uint32_t i = FindSlot(&domain);
    if (i != kNoSlot)
        ReleaseSlot(*mSlots[i]);
}

uint32_t DomainStateRelayModule::AttachedCount() const
{
    uint32_t n = 0;
    for (const auto& s : mSlots)
        if (s->adapter)
            ++n;
    return n;
}

uint32_t DomainStateRelayModule::PublishCount(const Dia::VisualDebugger::IDebugDomain& domain) const
{
    const uint32_t i = FindSlot(&domain);
    return (i != kNoSlot && mSlots[i]->publisher) ? mSlots[i]->publisher->PublishCount() : 0u;
}

void DomainStateRelayModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    for (auto& s : mSlots)
    {
        if (!s->publisher)
            continue;
        // Any demand event since the last tick (the Main-side adapter was asked for the state) arms the publisher.
        Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::CellLayers::ViewerStateDemandEvent>, 8> demand;
        s->demandIn.Consume(demand);
        if (demand.Size() > 0)
            s->publisher->Arm(ctx.tick);
        s->publisher->Update(ctx.tick);
    }
}

Dia::ApplicationFlow::StopResult DomainStateRelayModule::DoStop()
{
    for (auto& s : mSlots)
        ReleaseSlot(*s);
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace Cluiche::AppFlow

namespace { using DomainStateRelayModule_ = Cluiche::AppFlow::DomainStateRelayModule; }
DIA_MODULE(DomainStateRelayModule_);
DIA_DESCRIBE(DomainStateRelayModule_::kTypeId, "Sim end of the pooled debug-domain state snapshot: publishes domain state while demanded.");

#endif // DIA_DEBUG
