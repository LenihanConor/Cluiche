#pragma once
#include <stdio.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Core/Log.h>
#include <DiaMailbox/MailboxTypes.h>

namespace Dia::Mailbox {

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
        // Warn callback
        // ------------------------------------------------------------------
        void (*mWarnFn)(const char*) = nullptr;

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
        desc->head   = (desc->head + snapshotCount) % desc->capacity;
        desc->count -= snapshotCount;

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

} // namespace Dia::Mailbox
