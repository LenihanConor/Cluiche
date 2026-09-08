#include <DiaMessageBus/BusSubscriptionHandle.h>
#include <DiaMessageBus/Bus.h>

namespace Dia::MessageBus {

    BusSubscriptionHandle::BusSubscriptionHandle(Bus* bus, Dia::Core::Handle<HandlerRecord> handle)
        : mBus(bus), mHandle(handle) {}

    BusSubscriptionHandle::~BusSubscriptionHandle() {
        Release();
    }

    BusSubscriptionHandle::BusSubscriptionHandle(BusSubscriptionHandle&& other) noexcept
        : mBus(other.mBus), mHandle(other.mHandle) {
        other.mBus    = nullptr;
        other.mHandle = Dia::Core::Handle<HandlerRecord>();
    }

    BusSubscriptionHandle& BusSubscriptionHandle::operator=(BusSubscriptionHandle&& other) noexcept {
        if (this != &other) {
            Release();
            mBus          = other.mBus;
            mHandle       = other.mHandle;
            other.mBus    = nullptr;
            other.mHandle = Dia::Core::Handle<HandlerRecord>();
        }
        return *this;
    }

    bool BusSubscriptionHandle::IsValid() const {
        return mBus != nullptr;
    }

    void BusSubscriptionHandle::Release() {
        if (mBus != nullptr) {
            mBus->ReleaseHandlerSlot(mHandle);
            mBus = nullptr;
        }
    }

} // namespace Dia::MessageBus
