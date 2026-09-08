#include <DiaMailbox/Mailbox.h>
#include <DiaMailbox/IMailboxRouter.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia::Mailbox {

    Mailbox::Mailbox()
        : mWarnFn(nullptr)
    {
        auto& reg      = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricSent    = reg.RegisterCounter(Dia::Core::StringCRC("dia.mailbox.sent"));
        mMetricDropped = reg.RegisterCounter(Dia::Core::StringCRC("dia.mailbox.dropped"));
        mMetricDrained = reg.RegisterCounter(Dia::Core::StringCRC("dia.mailbox.drained"));
    }

    Mailbox::~Mailbox() {
        // Destruct all live slots and free each descriptor
        for (uint32_t i = 0; i < mRegistry.Size(); ++i) {
            TypedQueueDescriptor* desc = mRegistry[i];
            if (desc == nullptr) { continue; }

            // Destruct all live T and Address objects in the ring.
            // destructFn handles both T and Address (see SlotDestruct<T>).
            for (uint32_t j = 0; j < desc->count; ++j) {
                const uint32_t slotIdx = (desc->head + j) % desc->capacity;
                uint8_t* slot = desc->slotBuffer + (slotIdx * desc->slotStride);
                desc->destructFn(slot, desc->tOffset);
            }

            delete[] desc->slotBuffer;
            desc->slotBuffer = nullptr;
            delete desc;
        }
    }

    void Mailbox::SetWarnCallback(void(*fn)(const char*)) {
        mWarnFn = fn;
    }

    Mailbox::TypedQueueDescriptor* Mailbox::FindDescriptor(uint32_t key) {
        for (uint32_t i = 0; i < mRegistry.Size(); ++i) {
            if (mRegistry[i]->typeKey == key) {
                return mRegistry[i];
            }
        }
        return nullptr;
    }

    void Mailbox::EmitWarning(const char* msg) {
        if (mWarnFn != nullptr) {
            mWarnFn(msg);
        } else {
            DIA_LOG_WARNING("Mailbox", "%s", msg);
        }
    }

    Mailbox::TypeStats Mailbox::GetTypeStatsByIndex(int typeIndex) const {
        if (typeIndex < 0 || static_cast<uint32_t>(typeIndex) >= mRegistry.Size()) {
            return TypeStats{};
        }
        const TypedQueueDescriptor* desc = mRegistry[static_cast<uint32_t>(typeIndex)];
        TypeStats s;
        s.typeKey      = desc->typeKey;
        s.capacity     = desc->capacity;
        s.currentCount = desc->count;
        s.totalSent    = desc->totalSent;
        s.totalDropped = desc->totalDropped;
        s.totalDrained = desc->totalDrained;
        return s;
    }

    uint32_t Mailbox::GetRegisteredTypeCount() const {
        return mRegistry.Size();
    }

    uint32_t Mailbox::GetRouterCount() const {
        return mRouters.Size();
    }

    bool Mailbox::RegisterRouter(IMailboxRouter* router) {
        if (router == nullptr) { return false; }
        const Dia::Core::StringCRC routerId = router->GetRouterId();

        // Duplicate check
        for (uint32_t i = 0; i < mRouters.Size(); ++i) {
            if (mRouters[i]->GetRouterId() == routerId) { return false; }
        }

        if (mRouters.IsFull()) {
            char buf[256];
            sprintf_s(buf, sizeof(buf),
                "[DiaMailbox] RegisterRouter: router table full (capacity %u)", kMaxRouters);
            EmitWarning(buf);
            return false;
        }

        mRouters.Add(router);
        return true;
    }

    IMailboxRouter* Mailbox::GetRouter(Dia::Core::StringCRC routerId) {
        for (uint32_t i = 0; i < mRouters.Size(); ++i) {
            if (mRouters[i]->GetRouterId() == routerId) {
                return mRouters[i];
            }
        }
        return nullptr;
    }

    void Mailbox::Unsubscribe(SubscriptionHandle handle) {
        // Stale or default-constructed handle — no-op.
        if (!mSubscriptionPool.IsValid(handle.mHandle)) {
            return;
        }

        Subscription* sub = mSubscriptionPool.Get(handle.mHandle);
        if (sub == nullptr) { return; }

        // Remove from the per-type subscriber list.
        TypedQueueDescriptor* desc = FindDescriptor(sub->typeKey);
        if (desc != nullptr) {
            desc->subscriberList.RemoveFirst(sub->subscriberId);
        }

        mSubscriptionPool.Free(handle.mHandle);
    }

} // namespace Dia::Mailbox
