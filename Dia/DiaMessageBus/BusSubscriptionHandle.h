#pragma once
#include <DiaCore/Containers/Handle.h>

namespace Dia::MessageBus {

    class Bus;
    struct HandlerRecord;

    // ======================================================================
    // BusSubscriptionHandle
    //
    // RAII handle returned by Bus::Subscribe<T>. Unlike Mailbox::SubscriptionHandle
    // (which is a plain validity check — callers unsubscribe manually), this
    // handle actively unsubscribes on destruction: it removes the Bus-side
    // handler slot and unsubscribes the underlying Mailbox subscription.
    //
    // Move-only. PRECONDITION: the issuing Bus must outlive this handle.
    // ======================================================================
    class BusSubscriptionHandle {
    public:
        BusSubscriptionHandle() = default;
        ~BusSubscriptionHandle();

        BusSubscriptionHandle(const BusSubscriptionHandle&)            = delete;
        BusSubscriptionHandle& operator=(const BusSubscriptionHandle&) = delete;

        BusSubscriptionHandle(BusSubscriptionHandle&& other) noexcept;
        BusSubscriptionHandle& operator=(BusSubscriptionHandle&& other) noexcept;

        bool IsValid() const;

    private:
        friend class Bus;

        BusSubscriptionHandle(Bus* bus, Dia::Core::Handle<HandlerRecord> handle);

        void Release();

        Bus*                             mBus = nullptr;
        Dia::Core::Handle<HandlerRecord> mHandle;
    };

} // namespace Dia::MessageBus
