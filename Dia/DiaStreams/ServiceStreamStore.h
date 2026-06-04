#pragma once
#include <atomic>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include "IStreamStore.h"

namespace Dia { namespace ApplicationFlow {

// ServiceStreamStore<T>
// ---------------------------------------------------------------------------
// Collected-then-committed store for stable lifecycle handles.
// Provider calls Register(handle) from DoStart — handle is stored but
// NOT yet accessible.  Framework calls Commit() after all providers in the
// scope have registered — only then does Get() return the handle.
// Reset() clears handle + committed flag (called on stage unload for
// stage-scoped stores; never called for global-scoped stores).
// ---------------------------------------------------------------------------
template<typename T>
class ServiceStreamStore : public IStreamStore
{
public:
    ServiceStreamStore(const Dia::Core::StringCRC& id,
                       const Dia::Core::StringCRC& payloadType);

    const Dia::Core::StringCRC& GetId()          const override { return mId; }
    StreamKind                  GetKind()         const override { return StreamKind::kService; }
    const Dia::Core::StringCRC& GetPayloadType()  const override { return mPayloadType; }
    unsigned int                GetMaxReaders()   const override { return 0; }  // N/A for ServiceStream

    // IStreamStore overrides for ServiceStream commit gate
    bool IsCommitted()  const override { return mCommitted.load(std::memory_order_acquire); }
    bool IsRegistered() const override { return mHandle != nullptr; }
    void Commit()             override;
    void Reset()              override;

    // Called by providing module's ServiceStreamWriter from DoStart.
    // Asserts if called more than once without a Reset() in between.
    void Register(T& handle);

    // Consumer accessor — asserts if not yet committed.
    T& Get() const;

private:
    Dia::Core::StringCRC  mId;
    Dia::Core::StringCRC  mPayloadType;
    T*                    mHandle    = nullptr;
    std::atomic<bool>     mCommitted{false};
};

// --- inline implementation ---

template<typename T>
inline ServiceStreamStore<T>::ServiceStreamStore(const Dia::Core::StringCRC& id,
                                                  const Dia::Core::StringCRC& payloadType)
    : mId(id), mPayloadType(payloadType)
{}

template<typename T>
inline void ServiceStreamStore<T>::Register(T& handle)
{
    DIA_ASSERT(mHandle == nullptr,
        "ServiceStreamStore '%s' Register called more than once without Reset", mId.AsChar());
    mHandle = &handle;
    DIA_LOG_INFO("stream", "service_stream.registered stream_id=%s", mId.AsChar());
}

template<typename T>
inline void ServiceStreamStore<T>::Commit()
{
    DIA_ASSERT(mHandle != nullptr,
        "ServiceStreamStore '%s' Commit called but no handle was registered", mId.AsChar());
    mCommitted.store(true, std::memory_order_release);
    DIA_LOG_INFO("stream", "service_stream.committed stream_id=%s", mId.AsChar());
}

template<typename T>
inline T& ServiceStreamStore<T>::Get() const
{
    DIA_ASSERT(mCommitted.load(std::memory_order_acquire),
        "ServiceStreamStore '%s' Get called before Commit — consumer cannot access handle before framework commits", mId.AsChar());
    DIA_ASSERT(mHandle != nullptr, "ServiceStreamStore '%s' Get — handle is null after commit (internal error)", mId.AsChar());
    return *mHandle;
}

template<typename T>
inline void ServiceStreamStore<T>::Reset()
{
    mCommitted.store(false, std::memory_order_release);
    mHandle = nullptr;
    DIA_LOG_INFO("stream", "service_stream.reset stream_id=%s", mId.AsChar());
}

}} // namespace Dia::ApplicationFlow
