#pragma once
#include <cstdint>
#include <functional>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HandlePool.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaMailbox/Mailbox.h>
#include <DiaMessageBus/BusTypes.h>
#include <DiaMessageBus/LedgerSnapshot.h>
#include <DiaMessageBus/BusSubscriptionHandle.h>
#include <DiaMessageBus/IFlushAdapter.h>
#include <DiaMessageBus/BroadcastRouter.h>

namespace Dia::MessageBus {

    // ======================================================================
    // Type-erased handler storage.
    //
    // Physically these live in this header (Subscribe<T> is a template
    // instantiated per-T from arbitrary call sites, so HandlerSlot<T> must be
    // visible wherever that instantiation happens) but are not part of the
    // Bus's conceptual public surface — callers never touch IHandlerSlot,
    // HandlerSlot<T>, or HandlerRecord directly. std::function usage here is
    // the carve-out the system spec grants for handler storage internals.
    // ======================================================================
    class IHandlerSlot {
    public:
        virtual ~IHandlerSlot() = default;
    };

    template <class T>
    class HandlerSlot : public IHandlerSlot {
    public:
        std::function<void(const T&)> handler;
    };

    // Bus-side record of one Subscribe<T> call.
    struct HandlerRecord {
        IHandlerSlot*                     slot           = nullptr;
        Pass                              pass           = Pass::Primary;
        Dia::Mailbox::SubscriberId        subscriberId;
        uint32_t                          mailboxTypeKey = 0;
        Dia::Mailbox::SubscriptionHandle  mailboxHandle;
        bool                              active         = false;
    };

    // ======================================================================
    // Bus
    //
    // Wraps a single Dia::Mailbox::Mailbox instance with:
    //   - type-erased per-tick drain (RegisterType/DrainTrampoline)
    //   - a Bus-side handler table (Subscribe/DispatchOne), independent of
    //     Mailbox's own subscriber-id bookkeeping
    //   - a last-tick, double-buffered ledger
    //
    // Single-threaded (sim thread only), matching DiaMailbox SD-MBX-007 /
    // DiaMessageBus SD-MBX2-008.
    //
    // Update() runs two sweeps per tick: a Primary pass (dispatches to
    // Pass::Primary subscribers; these handlers may Post/Broadcast new
    // messages), then a Reaction pass (dispatches to Pass::Reaction
    // subscribers, including anything Posted during Primary). Post is
    // blocked — DIA_ASSERT in Debug, silent false in Release — while the
    // Reaction pass is executing, so a Reaction-pass handler cannot
    // re-enqueue and chain into a further sweep.
    // ======================================================================
    class Bus {
    public:
        Bus();
        ~Bus();

        Bus(const Bus&)            = delete;
        Bus& operator=(const Bus&) = delete;
        Bus(Bus&&)                 = delete;
        Bus& operator=(Bus&&)      = delete;

        // Router address constants.
        static const Dia::Core::StringCRC kBroadcastRouterId;
        static const Dia::Core::StringCRC kEntityRouterId;

        // Registers the owned BroadcastRouter with the owned Mailbox. Call
        // once, typically from MessageBusModule::DoStart().
        void Initialize();

        // Runs the pre-Primary flush-adapter sweep, then the Primary pass
        // (drain every registered type, resolve subscribers, dispatch), then
        // swaps the ledger double-buffer. Call once per sim tick, typically
        // from MessageBusModule::DoUpdate().
        void Update();

        // --- Registration (call during module start / connect-streams) ---

        // Registers T's typed queue with the owned Mailbox. Required before
        // Post/Subscribe<T>. Returns false if already registered or the
        // registry (Bus's own kMaxTypes, mirroring Mailbox::kMaxTypes) is full.
        template <class T, uint32_t kCapacity>
        bool RegisterType(Dia::Mailbox::OverflowPolicy policy = Dia::Mailbox::OverflowPolicy::DropOldest);

        // Records this system/component as a producer of type T. Graph
        // metadata only — zero routing effect.
        template <class T>
        void RegisterProducer(Dia::Core::StringCRC producerId);

        // Subscribes to messages of type T. handler is called during the
        // matching flush pass. Returns an invalid handle if T is unregistered,
        // the underlying Mailbox subscribe fails, or the handler pool is full.
        template <class T>
        BusSubscriptionHandle Subscribe(Dia::Core::StringCRC subscriberId,
                                         std::function<void(const T&)> handler,
                                         Pass pass = Pass::Primary);

        // Registers a flush adapter. Adapters are flushed in registration
        // order before the Primary pass. Caller keeps the adapter alive.
        void RegisterFlushAdapter(IFlushAdapter* adapter);

        // Registers a router (e.g. EntityRouter) with the owned Mailbox.
        void RegisterRouter(Dia::Mailbox::IMailboxRouter* router);

        // --- Posting (call from anywhere on sim thread during Update) ---

        // Enqueues a message. addr determines which router resolves delivery.
        // Returns false if T is unregistered.
        template <class T>
        bool Post(const Dia::Mailbox::Address& addr, const T& message);

        // Convenience: Post<T>({ kBroadcastRouterId, 0 }, message).
        template <class T>
        bool Broadcast(const T& message);

        // --- Ledger (diagnostic) ---

        // Read-only snapshot of the LAST COMPLETED tick's message flow.
        // Double-buffered: a read mid-tick returns the previous complete
        // tick, never the one currently being built.
        const LedgerSnapshot& GetLastTickLedger() const;

        // Diagnostic helper: true if a router with this id is registered
        // with the owned Mailbox.
        bool IsRouterRegistered(Dia::Core::StringCRC routerId);

    private:
        friend class BusSubscriptionHandle;

        static constexpr uint32_t kMaxTypes        = 32;  // mirrors Mailbox::kMaxTypes
        static constexpr uint32_t kMaxHandlers     = 256; // mirrors Mailbox::kMaxSubs
        static constexpr uint32_t kMaxFlushAdapters = 16;
        static constexpr uint32_t kMaxProducers    = 32;

        // Bus's OWN compiler-signature-based key, used only for Bus-side
        // bookkeeping (TypeRecord/HandlerRecord lookups). NOT the same
        // numeric value as Mailbox's internal (private) TypeKey<T>() — Bus
        // never needs to reproduce that value because every Mailbox call it
        // makes (RegisterType<T>, Send<T>, Drain<T>, Subscribe<T>, Resolve<T>)
        // already carries T at the call site, so Mailbox resolves its own key
        // internally and correctly on its own.
        template <class T>
        static uint32_t TypeKey() {
            static const uint32_t key = Dia::Core::StringCRC(__FUNCSIG__).Value();
            return key;
        }

        using DrainFn = void (*)(Bus& self, Pass currentPass, uint32_t maxCount);
        using CountFn = uint32_t (*)(Bus& self);

        struct TypeRecord {
            uint32_t             typeKey = 0;
            Dia::Core::StringCRC displayTypeId;
            DrainFn              drainFn = nullptr;
            CountFn              countFn = nullptr;
            // Mailbox::TypeStats::totalDropped is cumulative-since-registration,
            // not per-tick. We snapshot it at the end of every DrainTrampoline
            // call and diff against the running total next tick to recover the
            // this-tick delta, regardless of whether the drops happened before
            // Update() was even called or during a pre-Primary flush adapter.
            uint64_t              lastDroppedTotal = 0;
        };

        struct ProducerRecord {
            uint32_t             typeKey = 0;
            Dia::Core::StringCRC producerId;
        };

        template <class T>
        static void DrainTrampoline(Bus& self, Pass currentPass, uint32_t maxCount);

        template <class T>
        static uint32_t CountTrampoline(Bus& self) {
            return self.mMailbox.GetTypeStats<T>().currentCount;
        }

        template <class T>
        void DispatchOne(const Dia::Mailbox::Address& addr, const T& msg, Pass currentPass);

        // Called by BusSubscriptionHandle's destructor/move-assignment.
        void ReleaseHandlerSlot(Dia::Core::Handle<HandlerRecord> handle);

        LedgerMessageEntry& FindOrCreateLedgerEntry(Dia::Core::StringCRC typeId,
                                                     Dia::Core::StringCRC routerId,
                                                     Pass pass);

        TypeRecord* FindTypeRecord(uint32_t typeKey);

        Dia::Mailbox::Mailbox mMailbox;
        BroadcastRouter        mBroadcastRouter;

        Dia::Core::Containers::DynamicArrayC<TypeRecord, kMaxTypes>            mTypeRecords;
        Dia::Core::Containers::DynamicArrayC<ProducerRecord, kMaxProducers>    mProducerRecords;
        Dia::Core::Containers::DynamicArrayC<IFlushAdapter*, kMaxFlushAdapters> mFlushAdapters;

        Dia::Core::HandlePool<HandlerRecord, kMaxHandlers> mHandlerPool;

        LedgerSnapshot mLedgers[2];
        uint32_t       mBuildingIndex = 0;
        uint64_t       mTickCounter   = 0;

        // Re-entrancy guard: true only while the Reaction-pass sweep is
        // executing inside Update(). Blocks Post<T> from re-enqueuing during
        // that sweep (Primary-pass handlers are unaffected — they run before
        // this flag is set).
        bool mInReactionPass = false;
    };

    // ======================================================================
    // Template method implementations
    // ======================================================================

    template <class T, uint32_t kCapacity>
    bool Bus::RegisterType(Dia::Mailbox::OverflowPolicy policy) {
        const uint32_t key = TypeKey<T>();

        if (FindTypeRecord(key) != nullptr) {
            return false;
        }
        if (mTypeRecords.IsFull()) {
            return false;
        }
        if (!mMailbox.RegisterType<T, kCapacity>(policy)) {
            return false;
        }

        TypeRecord rec;
        rec.typeKey       = key;
        rec.displayTypeId = T::kTypeId;
        rec.drainFn       = &Bus::DrainTrampoline<T>;
        rec.countFn       = &Bus::CountTrampoline<T>;
        mTypeRecords.Add(rec);
        return true;
    }

    template <class T>
    void Bus::RegisterProducer(Dia::Core::StringCRC producerId) {
        const uint32_t key = TypeKey<T>();

        for (uint32_t i = 0; i < mProducerRecords.Size(); ++i) {
            if (mProducerRecords[i].typeKey == key) {
                mProducerRecords[i].producerId = producerId;
                return;
            }
        }
        if (mProducerRecords.IsFull()) {
            DIA_LOG_WARNING("DiaMessageBus", "RegisterProducer: producer table full");
            return;
        }

        ProducerRecord rec;
        rec.typeKey    = key;
        rec.producerId = producerId;
        mProducerRecords.Add(rec);
    }

    template <class T>
    BusSubscriptionHandle Bus::Subscribe(Dia::Core::StringCRC subscriberId,
                                          std::function<void(const T&)> handler,
                                          Pass pass) {
        const uint32_t key = TypeKey<T>();

        if (FindTypeRecord(key) == nullptr) {
            DIA_LOG_WARNING("DiaMessageBus", "Subscribe: type not registered");
            return BusSubscriptionHandle();
        }

        Dia::Mailbox::SubscriberId mailboxSubscriberId;
        mailboxSubscriberId.value = static_cast<uint64_t>(subscriberId.Value());

        Dia::Mailbox::SubscriptionHandle mailboxHandle = mMailbox.Subscribe<T>(mailboxSubscriberId);
        if (!mailboxHandle.IsValid()) {
            DIA_LOG_WARNING("DiaMessageBus", "Subscribe: underlying Mailbox subscribe failed");
            return BusSubscriptionHandle();
        }

        Dia::Core::Handle<HandlerRecord> poolHandle = mHandlerPool.Allocate();
        if (!mHandlerPool.IsValid(poolHandle)) {
            mMailbox.Unsubscribe(mailboxHandle);
            DIA_LOG_WARNING("DiaMessageBus", "Subscribe: Bus handler pool full");
            return BusSubscriptionHandle();
        }

        HandlerSlot<T>* slot = new HandlerSlot<T>();
        slot->handler = handler;

        HandlerRecord* rec  = mHandlerPool.Get(poolHandle);
        rec->slot           = slot;
        rec->pass           = pass;
        rec->subscriberId   = mailboxSubscriberId;
        rec->mailboxTypeKey = key;
        rec->mailboxHandle  = mailboxHandle;
        rec->active         = true;

        return BusSubscriptionHandle(this, poolHandle);
    }

    template <class T>
    bool Bus::Post(const Dia::Mailbox::Address& addr, const T& message) {
        if (mInReactionPass) {
            DIA_ASSERT(false, "DiaMessageBus: Post/Broadcast called during the Reaction pass — Reaction handlers may not re-enqueue");
            return false;
        }

        const bool ok = mMailbox.Send<T>(addr, message);
        if (!ok) {
            DIA_LOG_WARNING("DiaMessageBus", "Post: send failed (type not registered?)");
        }
        return ok;
    }

    template <class T>
    bool Bus::Broadcast(const T& message) {
        return Post<T>(Dia::Mailbox::Address{ kBroadcastRouterId, 0 }, message);
    }

    template <class T>
    void Bus::DrainTrampoline(Bus& self, Pass currentPass, uint32_t maxCount) {
        self.mMailbox.Drain<T>([&self, currentPass](const Dia::Mailbox::Address& addr, const T& msg) {
            self.DispatchOne<T>(addr, msg, currentPass);
        }, maxCount);

        // Recover this-tick's drop delta from the cumulative Mailbox total,
        // regardless of when the drops happened (before Update() was called,
        // or during this tick's pre-Primary flush adapters).
        TypeRecord* rec = self.FindTypeRecord(TypeKey<T>());
        if (rec != nullptr) {
            const uint64_t totalDroppedNow = self.mMailbox.GetTypeStats<T>().totalDropped;
            const uint64_t deltaThisTick   = totalDroppedNow - rec->lastDroppedTotal;
            rec->lastDroppedTotal = totalDroppedNow;
            self.mLedgers[self.mBuildingIndex].droppedCount += static_cast<uint32_t>(deltaThisTick);
        }
    }

    template <class T>
    void Bus::DispatchOne(const Dia::Mailbox::Address& addr, const T& msg, Pass currentPass) {
        const uint32_t key = TypeKey<T>();

        LedgerMessageEntry& entry = FindOrCreateLedgerEntry(T::kTypeId, addr.routerId, currentPass);
        entry.count += 1;

        Dia::Mailbox::SubscriberSet matched;
        const bool resolved = mMailbox.Resolve<T>(addr, matched);
        if (!resolved) {
            return;
        }

        for (uint32_t i = 0; i < matched.Size(); ++i) {
            const Dia::Mailbox::SubscriberId sid = matched[i];
            mHandlerPool.ForEach([&](Dia::Core::Handle<HandlerRecord> /*handle*/, HandlerRecord& rec) {
                if (rec.active && rec.mailboxTypeKey == key &&
                    rec.pass == currentPass && rec.subscriberId == sid) {
                    static_cast<HandlerSlot<T>*>(rec.slot)->handler(msg);
                    entry.deliveries += 1;
                }
            });
        }
    }

} // namespace Dia::MessageBus
