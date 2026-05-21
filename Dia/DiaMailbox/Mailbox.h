#pragma once
#include <stdio.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaCore/Containers/HandlePool.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaMailbox/MailboxTypes.h>
#include <DiaMailbox/Subscription.h>
#include <DiaMailbox/IMailboxRouter.h>

namespace Dia::Mailbox {

    // ======================================================================
    // SubscriptionHandle
    // Defined before Mailbox so it can be used as a value parameter.
    // PRECONDITION: the issuing Mailbox must outlive any SubscriptionHandle it
    // produces. Calling IsValid() after Mailbox destruction is undefined behaviour.
    // ======================================================================
    class SubscriptionHandle {
    public:
        SubscriptionHandle() = default;

        bool IsValid() const {
            if (mPool == nullptr) { return false; }
            return mPool->IsValid(mHandle);
        }

    private:
        friend class Mailbox;

        SubscriptionHandle(const Dia::Core::HandlePool<Subscription, kMaxSubs>* pool,
                           Dia::Core::Handle<Subscription> handle)
            : mPool(pool), mHandle(handle) {}

        const Dia::Core::HandlePool<Subscription, kMaxSubs>* mPool   = nullptr;
        Dia::Core::Handle<Subscription>                      mHandle;
    };

    class Mailbox {
    public:
        Mailbox();
        ~Mailbox();

        Mailbox(const Mailbox&)            = delete;
        Mailbox& operator=(const Mailbox&) = delete;
        Mailbox(Mailbox&&)                 = delete;
        Mailbox& operator=(Mailbox&&)      = delete;

        // Override warning output for tests. Pass nullptr to restore default.
        void SetWarnCallback(void(*fn)(const char*));

        // Register a typed queue with fixed capacity.
        // Returns true on first registration; false if already registered or registry full.
        template <class T, uint32_t kCapacity>
        bool RegisterType(OverflowPolicy policy = OverflowPolicy::DropOldest);

        // Send a message to all subscribers of type T.
        // Returns false if type is not registered.
        template <class T>
        bool Send(const Address& addr, const T& message);

        // Drain all messages of type T, calling visitor(addr, msg) for each.
        // Snapshot semantics: visits only messages present at call time.
        // Messages sent during drain are preserved for the next drain.
        // No-op (no warning) if type is not registered.
        template <class T, class Visitor>
        void Drain(const Visitor& visitor);

        // Subscribe subscriber to messages of type T.
        // Returns an invalid handle if T is not registered, the per-type list is full, or the pool is full.
        template <class T>
        SubscriptionHandle Subscribe(SubscriberId subscriber);

        // Unsubscribe using the handle returned by Subscribe.
        // No-op (no crash) if the handle is stale or default-constructed.
        void Unsubscribe(SubscriptionHandle handle);

        // Return the set of subscriber IDs registered for type T.
        // Returns an empty set (no crash) if T is not registered.
        template <class T>
        const SubscriberSet& GetSubscribersForType() const;

        // Cumulative stats for a single registered type.
        struct TypeStats {
            uint32_t typeKey      = 0;
            uint32_t capacity     = 0;
            uint32_t currentCount = 0;  // live messages in queue right now
            uint64_t totalSent    = 0;  // successful sends since registration
            uint64_t totalDropped = 0;  // messages dropped by overflow since registration
            uint64_t totalDrained = 0;  // messages drained (visited) since registration
        };

        // Per-type stats for T. Returns a zeroed TypeStats if T is not registered.
        template <class T>
        TypeStats GetTypeStats() const;

        // Number of registered types (0..kMaxTypes).
        uint32_t GetRegisteredTypeCount() const;

        // Number of registered routers (0..kMaxRouters).
        uint32_t GetRouterCount() const;

        // Register a router. Returns false on duplicate ID or full table.
        bool RegisterRouter(IMailboxRouter* router);

        // Look up a router by ID. Returns nullptr if not found.
        IMailboxRouter* GetRouter(Dia::Core::StringCRC routerId);

        // Resolve recipients for a typed message using a router.
        // Returns false (silent) if routerId is null/zero. Returns false with warning
        // if router is not registered. Otherwise calls router->Resolve and returns true.
        template <class T>
        bool Resolve(const Address& addr, SubscriberSet& outMatched);

    private:

        // ------------------------------------------------------------------
        // Slot stride computation
        // ------------------------------------------------------------------
        static uint32_t ComputeSlotStride(uint32_t tSize, uint32_t tAlign) {
            uint32_t addrSize    = static_cast<uint32_t>(sizeof(Address));
            uint32_t addrAligned = (addrSize + tAlign - 1u) & ~(tAlign - 1u);
            return addrAligned + tSize;
        }

        // ------------------------------------------------------------------
        // Per-type destructor: called for each live slot on shutdown and on drop.
        // Destructs both T and the Address stored in the slot.
        // ------------------------------------------------------------------
        using DestructFn = void(*)(uint8_t* slotPtr, uint32_t tOffset);

        template <class T>
        static void SlotDestruct(uint8_t* slotPtr, uint32_t tOffset) {
            T*       obj  = reinterpret_cast<T*>(slotPtr + tOffset);
            Address* addr = reinterpret_cast<Address*>(slotPtr);
            obj->~T();
            addr->~Address();
        }

        // ------------------------------------------------------------------
        // TypedQueueDescriptor — one per registered type
        // ------------------------------------------------------------------
        struct TypedQueueDescriptor {
            uint32_t      typeKey;
            uint32_t      capacity;
            uint32_t      slotStride;
            uint32_t      tOffset;    // byte offset within a slot where T begins
            uint32_t      head;       // index of oldest slot
            uint32_t      count;      // number of live slots
            uint32_t      dropsThisDrain; // drops accumulated since last real drain
            OverflowPolicy policy;
            uint8_t*      slotBuffer; // heap-allocated flat array [capacity * slotStride]
            DestructFn    destructFn;
            SubscriberSet subscriberList; // subscribers registered for this type
            // Cumulative observability counters (never reset after registration)
            uint64_t      totalSent    = 0;
            uint64_t      totalDropped = 0;
            uint64_t      totalDrained = 0;
        };

        // ------------------------------------------------------------------
        // Type key: unique per T, derived from function signature
        // ------------------------------------------------------------------
        template <class T>
        static uint32_t TypeKey() {
            static const uint32_t key = Dia::Core::StringCRC(__FUNCSIG__).Value();
            return key;
        }

        // ------------------------------------------------------------------
        // Registry
        // ------------------------------------------------------------------
        static constexpr uint32_t kMaxTypes = 32;
        Dia::Core::Containers::DynamicArrayC<TypedQueueDescriptor*, kMaxTypes> mRegistry;

        // ------------------------------------------------------------------
        // Router table
        // ------------------------------------------------------------------
        static constexpr uint32_t kMaxRouters = 16;
        Dia::Core::Containers::DynamicArrayC<IMailboxRouter*, kMaxRouters> mRouters;

        // ------------------------------------------------------------------
        // Subscription pool
        // ------------------------------------------------------------------
        Dia::Core::HandlePool<Subscription, kMaxSubs> mSubscriptionPool;

        // ------------------------------------------------------------------
        // Warn callback
        // ------------------------------------------------------------------
        void (*mWarnFn)(const char*) = nullptr;

        // ------------------------------------------------------------------
        // Metrics (registered on construction; owned by MetricRegistry)
        // ------------------------------------------------------------------
        Dia::Observation::Metric::Counter* mMetricSent    = nullptr;
        Dia::Observation::Metric::Counter* mMetricDropped = nullptr;
        Dia::Observation::Metric::Counter* mMetricDrained = nullptr;

        // ------------------------------------------------------------------
        // Private helpers (implemented in Mailbox.cpp)
        // ------------------------------------------------------------------
        TypedQueueDescriptor* FindDescriptor(uint32_t key);
        void EmitWarning(const char* msg);
    };

    // ======================================================================
    // Template method implementations
    // ======================================================================

    template <class T, uint32_t kCapacity>
    bool Mailbox::RegisterType(OverflowPolicy policy) {
        const uint32_t key = TypeKey<T>();

        // Already registered?
        if (FindDescriptor(key) != nullptr) {
            return false;
        }

        // Registry full?
        if (mRegistry.IsFull()) {
            return false;
        }

        const uint32_t tSize   = static_cast<uint32_t>(sizeof(T));
        const uint32_t tAlign  = static_cast<uint32_t>(alignof(T));
        const uint32_t stride  = ComputeSlotStride(tSize, tAlign);

        // Compute tOffset (where T lives inside a slot)
        const uint32_t addrSize    = static_cast<uint32_t>(sizeof(Address));
        const uint32_t addrAligned = (addrSize + tAlign - 1u) & ~(tAlign - 1u);

        TypedQueueDescriptor* desc = new TypedQueueDescriptor();
        desc->typeKey        = key;
        desc->capacity       = kCapacity;
        desc->slotStride     = stride;
        desc->tOffset        = addrAligned;
        desc->head           = 0;
        desc->count          = 0;
        desc->dropsThisDrain = 0;
        desc->policy         = policy;
        desc->slotBuffer     = new uint8_t[kCapacity * stride];
        desc->destructFn     = &SlotDestruct<T>;

        mRegistry.Add(desc);
        return true;
    }

    template <class T>
    bool Mailbox::Send(const Address& addr, const T& message) {
        const uint32_t key  = TypeKey<T>();
        TypedQueueDescriptor* desc = FindDescriptor(key);

        if (desc == nullptr) {
            char buf[256];
            sprintf_s(buf, sizeof(buf),
                "[DiaMailbox] Send: type not registered (key=0x%08X)", key);
            EmitWarning(buf);
            return false;
        }

        if (desc->count == desc->capacity) {
            // Queue full — apply overflow policy
            if (desc->policy == OverflowPolicy::DropOldest) {
                // Destruct the oldest slot (at head)
                uint8_t* oldSlot = desc->slotBuffer + (desc->head * desc->slotStride);
                desc->destructFn(oldSlot, desc->tOffset);

                // Advance head, shrink count, record drop
                desc->head = (desc->head + 1u) % desc->capacity;
                --desc->count;
                ++desc->dropsThisDrain;
                ++desc->totalDropped;
                if (mMetricDropped) { mMetricDropped->Inc(); }
            } else {
                // Assert policy: fire in DEBUG, silently return false in RELEASE
                DIA_ASSERT(false, "DiaMailbox: queue overflow (Assert policy), type key=0x%08X", key);
                return false;
            }
        }

        // Write into the next free slot: (head + count) % capacity
        const uint32_t slotIdx = (desc->head + desc->count) % desc->capacity;
        uint8_t* slot = desc->slotBuffer + (slotIdx * desc->slotStride);

        // Placement-copy the Address at offset 0
        new (slot) Address(addr);

        // Placement-copy T at tOffset
        new (slot + desc->tOffset) T(message);

        ++desc->count;
        ++desc->totalSent;
        if (mMetricSent) { mMetricSent->Inc(); }
        return true;
    }

    template <class T, class Visitor>
    void Mailbox::Drain(const Visitor& visitor) {
        const uint32_t key  = TypeKey<T>();
        TypedQueueDescriptor* desc = FindDescriptor(key);

        if (desc == nullptr) {
            // Type never registered — silent no-op (per AC13)
            return;
        }

        // Snapshot: visit only messages present right now
        const uint32_t snapshotCount = desc->count;

        if (snapshotCount == 0) {
            // Empty drain: no warning, dropsThisDrain NOT reset (per spec AI Q11)
            return;
        }

        // Visit each message in FIFO order
        for (uint32_t i = 0; i < snapshotCount; ++i) {
            const uint32_t slotIdx = (desc->head + i) % desc->capacity;
            uint8_t* slot = desc->slotBuffer + (slotIdx * desc->slotStride);

            // Const views for the visitor
            const Address& addrRef = *reinterpret_cast<const Address*>(slot);
            const T&       msgRef  = *reinterpret_cast<const T*>(slot + desc->tOffset);

            visitor(addrRef, msgRef);

            // Destruct via the type-erased function (destructs both T and Address)
            desc->destructFn(slot, desc->tOffset);
        }

        // Advance head past drained messages; preserve any new sends that arrived during drain
        desc->head         = (desc->head + snapshotCount) % desc->capacity;
        desc->count       -= snapshotCount;
        desc->totalDrained += snapshotCount;
        if (mMetricDrained) { mMetricDrained->Inc(snapshotCount); }

        // Emit drop warning if drops occurred during this drain window
        if (desc->dropsThisDrain > 0) {
            char buf[256];
            sprintf_s(buf, sizeof(buf),
                "[DiaMailbox] Drain: %u message(s) dropped due to overflow (type key=0x%08X)",
                desc->dropsThisDrain, desc->typeKey);
            EmitWarning(buf);
            desc->dropsThisDrain = 0;
        }
    }

    // ======================================================================
    // Subscribe<T> template implementation
    // ======================================================================
    template <class T>
    SubscriptionHandle Mailbox::Subscribe(SubscriberId subscriber) {
        const uint32_t key = TypeKey<T>();
        TypedQueueDescriptor* desc = FindDescriptor(key);

        if (desc == nullptr) {
            char buf[256];
            sprintf_s(buf, sizeof(buf),
                "[DiaMailbox] Subscribe: type not registered (key=0x%08X)", key);
            EmitWarning(buf);
            return SubscriptionHandle();
        }

        if (desc->subscriberList.IsFull()) {
            char buf[256];
            sprintf_s(buf, sizeof(buf),
                "[DiaMailbox] Subscribe: per-type subscriber list full (capacity 64, key=0x%08X)", key);
            EmitWarning(buf);
            return SubscriptionHandle();
        }

        if (mSubscriptionPool.IsFull()) {
            char buf[256];
            sprintf_s(buf, sizeof(buf),
                "[DiaMailbox] Subscribe: subscription pool full (capacity %u)", kMaxSubs);
            EmitWarning(buf);
            return SubscriptionHandle();
        }

        Dia::Core::Handle<Subscription> handle = mSubscriptionPool.Allocate();
        Subscription* sub = mSubscriptionPool.Get(handle);
        sub->subscriberId = subscriber;
        sub->typeKey      = key;

        desc->subscriberList.Add(subscriber);

        return SubscriptionHandle(&mSubscriptionPool, handle);
    }

    // ======================================================================
    // GetSubscribersForType<T> template implementation
    // ======================================================================
    template <class T>
    const SubscriberSet& Mailbox::GetSubscribersForType() const {
        const uint32_t key = TypeKey<T>();
        for (uint32_t i = 0; i < mRegistry.Size(); ++i) {
            if (mRegistry[i]->typeKey == key) {
                return mRegistry[i]->subscriberList;
            }
        }
        static const SubscriberSet kEmpty;
        return kEmpty;
    }

    // ======================================================================
    // Resolve<T> template implementation
    // ======================================================================
    template <class T>
    bool Mailbox::Resolve(const Address& addr, SubscriberSet& outMatched) {
        // Null router — valid no-routing use-case; silent false
        if (addr.routerId == Dia::Core::StringCRC{}) {
            outMatched.RemoveAll();
            return false;
        }

        IMailboxRouter* router = GetRouter(addr.routerId);
        if (router == nullptr) {
            char buf[256];
            sprintf_s(buf, sizeof(buf),
                "[DiaMailbox] Resolve: no router registered for routerId (key=0x%08X)",
                addr.routerId.Value());
            EmitWarning(buf);
            outMatched.RemoveAll();
            return false;
        }

        const SubscriberSet& live = GetSubscribersForType<T>();
        outMatched.RemoveAll();
        router->Resolve(addr, live, outMatched);
        return true;
    }

    // ======================================================================
    // GetTypeStats<T> template implementation
    // ======================================================================
    template <class T>
    Mailbox::TypeStats Mailbox::GetTypeStats() const {
        const uint32_t key = TypeKey<T>();
        for (uint32_t i = 0; i < mRegistry.Size(); ++i) {
            const TypedQueueDescriptor* desc = mRegistry[i];
            if (desc->typeKey == key) {
                TypeStats s;
                s.typeKey      = desc->typeKey;
                s.capacity     = desc->capacity;
                s.currentCount = desc->count;
                s.totalSent    = desc->totalSent;
                s.totalDropped = desc->totalDropped;
                s.totalDrained = desc->totalDrained;
                return s;
            }
        }
        return TypeStats{};
    }

} // namespace Dia::Mailbox
