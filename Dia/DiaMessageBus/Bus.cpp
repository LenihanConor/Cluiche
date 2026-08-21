#include <DiaMessageBus/Bus.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Dia::MessageBus {

    const Dia::Core::StringCRC Bus::kBroadcastRouterId("broadcast");
    const Dia::Core::StringCRC Bus::kEntityRouterId("entity");

    Bus::Bus()  = default;
    Bus::~Bus() = default;

    void Bus::Initialize() {
        mMailbox.RegisterRouter(&mBroadcastRouter);
    }

    void Bus::Update() {
        DIA_TRACE_ZONE("messagebus.flush", ::Dia::Observation::Trace::Category::kNone);

        LedgerSnapshot& building = mLedgers[mBuildingIndex];
        building.entries.RemoveAll();
        building.droppedCount = 0;
        building.tickIndex    = mTickCounter++;
        // No wall-clock helper is wired into DiaMessageBus yet; 0 is a
        // deliberate placeholder (see task brief / Observation follow-ups).
        building.timestampUs  = 0;

        // Pre-Primary: flush adapters, in registration order.
        for (uint32_t i = 0; i < mFlushAdapters.Size(); ++i) {
            if (mFlushAdapters[i] != nullptr) {
                mFlushAdapters[i]->Flush(*this);
            }
        }

        // Primary pass: drain every registered type.
        for (uint32_t i = 0; i < mTypeRecords.Size(); ++i) {
            mTypeRecords[i].drainFn(*this, Pass::Primary);
        }

        // Reaction pass: drain every registered type again, dispatching only
        // to Reaction-pass subscribers. Primary-pass handlers above may have
        // Post/Broadcast'd new messages — those are visible here since Post
        // writes straight into the Mailbox type queue. While this sweep runs,
        // Post is blocked (see Bus::Post) so Reaction handlers cannot chain
        // into a further sweep.
        mInReactionPass = true;
        for (uint32_t i = 0; i < mTypeRecords.Size(); ++i) {
            mTypeRecords[i].drainFn(*this, Pass::Reaction);
        }
        mInReactionPass = false;

        // Swap: the buffer just finished building becomes "last tick".
        mBuildingIndex = 1 - mBuildingIndex;
    }

    void Bus::RegisterFlushAdapter(IFlushAdapter* adapter) {
        if (adapter == nullptr) {
            return;
        }
        if (mFlushAdapters.IsFull()) {
            DIA_LOG_WARNING("DiaMessageBus", "RegisterFlushAdapter: table full (capacity %u)", kMaxFlushAdapters);
            return;
        }
        mFlushAdapters.Add(adapter);
    }

    void Bus::RegisterRouter(Dia::Mailbox::IMailboxRouter* router) {
        mMailbox.RegisterRouter(router);
    }

    const LedgerSnapshot& Bus::GetLastTickLedger() const {
        return mLedgers[1 - mBuildingIndex];
    }

    bool Bus::IsRouterRegistered(Dia::Core::StringCRC routerId) {
        return mMailbox.GetRouter(routerId) != nullptr;
    }

    Bus::TypeRecord* Bus::FindTypeRecord(uint32_t typeKey) {
        for (uint32_t i = 0; i < mTypeRecords.Size(); ++i) {
            if (mTypeRecords[i].typeKey == typeKey) {
                return &mTypeRecords[i];
            }
        }
        return nullptr;
    }

    void Bus::ReleaseHandlerSlot(Dia::Core::Handle<HandlerRecord> handle) {
        if (!mHandlerPool.IsValid(handle)) {
            return;
        }

        HandlerRecord* rec = mHandlerPool.Get(handle);
        if (rec != nullptr) {
            delete rec->slot;
            rec->slot = nullptr;
            if (rec->mailboxHandle.IsValid()) {
                mMailbox.Unsubscribe(rec->mailboxHandle);
            }
            rec->active = false;
        }
        mHandlerPool.Free(handle);
    }

    LedgerMessageEntry& Bus::FindOrCreateLedgerEntry(Dia::Core::StringCRC typeId,
                                                      Dia::Core::StringCRC routerId,
                                                      Pass pass) {
        LedgerSnapshot& building = mLedgers[mBuildingIndex];

        for (uint32_t i = 0; i < building.entries.Size(); ++i) {
            if (building.entries[i].typeId == typeId && building.entries[i].pass == pass) {
                return building.entries[i];
            }
        }

        LedgerMessageEntry entry;
        entry.typeId     = typeId;
        entry.routerId   = routerId;
        entry.count      = 0;
        entry.deliveries = 0;
        entry.pass       = pass;

        if (building.entries.IsFull()) {
            // 32 types x 2 passes == 64 capacity; this task only dispatches
            // Primary, so this should never trip. Guard anyway.
            DIA_LOG_WARNING("DiaMessageBus", "Ledger entries full — dropping tally entry for this tick");
            static LedgerMessageEntry sOverflowSink;
            sOverflowSink = entry;
            return sOverflowSink;
        }

        building.entries.Add(entry);
        return building.entries[building.entries.Size() - 1];
    }

} // namespace Dia::MessageBus
