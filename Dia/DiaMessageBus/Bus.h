#pragma once
#include <cstdint>
#include <functional>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HandlePool.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>
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
        // Display-only copy of the StringCRC passed to Subscribe<T>.
        // subscriberId above is Mailbox's own numeric SubscriberId (built
        // from StringCRC::Value()) and cannot be turned back into a string;
        // this field exists purely so debug/tooling introspection (e.g.
        // MessageBusDebugDomain's Schema tab, see ForEachSubscriberForType
        // below) can show a human-readable subscriber id. Not read by any
        // routing/dispatch code path.
        Dia::Core::StringCRC              displaySubscriberId;
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

        // --- Health/capacity introspection (Release-safe — used by
        // BusHealthReporter and available to any caller that wants to watch
        // for the bus approaching a fixed-capacity limit) ---
        uint32_t GetRegisteredTypeCount() const { return mTypeRecords.Size(); }
        uint32_t GetTypeCapacity()        const { return kMaxTypes; }
        uint32_t GetHandlerCount()        const { return mHandlerPool.GetSize(); }
        uint32_t GetHandlerCapacity()     const { return kMaxHandlers; }

        // --- Debug-only introspection (tooling, e.g. MessageBusDebugDomain's
        // Schema tab) ---
        //
        // Bus has no notion of "this type's router" at RegisterType time —
        // routing is decided per-Post via the Address passed by the caller,
        // not declared per-type. Rather than bolt a router parameter onto
        // RegisterType<T>/RegisterProducer<T> (both are codegen'd and called
        // from many production sites — see DiaMessageBus/Messages/*.h — so
        // widening either signature is not "minimal and targeted"), these
        // accessors expose which router(s) a type has actually been
        // dispatched through so far. That is graph metadata Bus already has
        // the raw data for (DispatchOne sees addr.routerId every call); it
        // is just not currently surfaced. Kept out of Release entirely since
        // only the Debug-only visual debugger domain consumes it.
#ifdef DIA_DEBUG
        struct RegisteredTypeInfo {
            Dia::Core::StringCRC typeId;
            bool                 sawBroadcastRouter = false;
            bool                 sawEntityRouter    = false;
        };

        // Visits every registered type (registration order), each with the
        // set of routers it has been dispatched through so far.
        template <class Fn>
        void ForEachRegisteredType(Fn&& fn) const;

        // Visits every producer id registered (RegisterProducer<T>) against
        // the type whose display id is typeId.
        template <class Fn>
        void ForEachProducerForType(Dia::Core::StringCRC typeId, Fn&& fn) const;

        // Visits every currently-active subscriber id (Subscribe<T>) for the
        // type whose display id is typeId.
        template <class Fn>
        void ForEachSubscriberForType(Dia::Core::StringCRC typeId, Fn&& fn) const;
#endif // DIA_DEBUG

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
            // Debug-only introspection (see ForEachRegisteredType): which
            // router(s) this type has been dispatched through so far, ever.
            // Updated from DispatchOne<T>; never reset. Always compiled
            // (like the rest of TypeRecord) so DispatchOne doesn't need an
            // #ifdef in the dispatch hot path — only the public accessors
            // that read these flags are Debug-only.
            bool                  sawBroadcastRouter = false;
            bool                  sawEntityRouter    = false;
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
#ifdef DIA_DEBUG
        // Debug-only introspection helper: linear scan by display id rather
        // than by internal typeKey, since tooling only ever has the
        // StringCRC a type registered itself under (T::kTypeId).
        const TypeRecord* FindTypeRecordByDisplayId(Dia::Core::StringCRC typeId) const;
#endif

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

        // dia.msgbus.{posted,delivered,dropped} — registered in the
        // constructor, mirroring Mailbox's own mMetricSent/mMetricDropped/
        // mMetricDrained pattern (Mailbox.cpp). Always compiled (Release
        // too) — these are cheap, sustained-operations metrics, not
        // Debug-only diagnostics.
        Dia::Observation::Metric::Counter* mMetricPosted    = nullptr;
        Dia::Observation::Metric::Counter* mMetricDelivered = nullptr;
        Dia::Observation::Metric::Counter* mMetricDropped   = nullptr;
    };

    // ======================================================================
    // Template method implementations
    // ======================================================================

    template <class T, uint32_t kCapacity>
    bool Bus::RegisterType(Dia::Mailbox::OverflowPolicy policy) {
        const uint32_t key = TypeKey<T>();

        if (FindTypeRecord(key) != nullptr) {
            // Already registered — expected/idempotent (codegen'd
            // RegisterMessages() call sites rely on this being a silent
            // no-op), not a warning-worthy condition.
            return false;
        }
        if (mTypeRecords.IsFull()) {
            DIA_LOG_WARNING("DiaMessageBus", "RegisterType: type registry full (capacity %u)", kMaxTypes);
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
        rec->displaySubscriberId = subscriberId;

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
        } else if (mMetricPosted) {
            mMetricPosted->Inc();
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
            if (self.mMetricDropped && deltaThisTick > 0) {
                self.mMetricDropped->Inc(deltaThisTick);
            }
        }
    }

    template <class T>
    void Bus::DispatchOne(const Dia::Mailbox::Address& addr, const T& msg, Pass currentPass) {
        const uint32_t key = TypeKey<T>();

        LedgerMessageEntry& entry = FindOrCreateLedgerEntry(T::kTypeId, addr.routerId, currentPass);
        entry.count += 1;

        // Debug-only introspection bookkeeping (see ForEachRegisteredType) —
        // cheap (two StringCRC compares), always compiled, no #ifdef in this
        // hot path; only the public accessors reading these flags are
        // Debug-only.
        if (TypeRecord* rec = FindTypeRecord(key)) {
            if (addr.routerId == kBroadcastRouterId) {
                rec->sawBroadcastRouter = true;
            } else if (addr.routerId == kEntityRouterId) {
                rec->sawEntityRouter = true;
            }
        }

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
                    if (mMetricDelivered) {
                        mMetricDelivered->Inc();
                    }
                }
            });
        }
    }

#ifdef DIA_DEBUG
    template <class Fn>
    void Bus::ForEachRegisteredType(Fn&& fn) const {
        for (uint32_t i = 0; i < mTypeRecords.Size(); ++i) {
            const TypeRecord& rec = mTypeRecords[i];
            RegisteredTypeInfo info;
            info.typeId             = rec.displayTypeId;
            info.sawBroadcastRouter = rec.sawBroadcastRouter;
            info.sawEntityRouter    = rec.sawEntityRouter;
            fn(info);
        }
    }

    template <class Fn>
    void Bus::ForEachProducerForType(Dia::Core::StringCRC typeId, Fn&& fn) const {
        const TypeRecord* typeRec = FindTypeRecordByDisplayId(typeId);
        if (typeRec == nullptr) {
            return;
        }
        for (uint32_t i = 0; i < mProducerRecords.Size(); ++i) {
            if (mProducerRecords[i].typeKey == typeRec->typeKey) {
                fn(mProducerRecords[i].producerId);
            }
        }
    }

    template <class Fn>
    void Bus::ForEachSubscriberForType(Dia::Core::StringCRC typeId, Fn&& fn) const {
        const TypeRecord* typeRec = FindTypeRecordByDisplayId(typeId);
        if (typeRec == nullptr) {
            return;
        }
        mHandlerPool.ForEach([&](Dia::Core::Handle<HandlerRecord> /*handle*/, const HandlerRecord& rec) {
            if (rec.active && rec.mailboxTypeKey == typeRec->typeKey) {
                fn(rec.displaySubscriberId);
            }
        });
    }
#endif // DIA_DEBUG

} // namespace Dia::MessageBus
