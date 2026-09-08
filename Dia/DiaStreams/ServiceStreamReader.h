#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/Core/Assert.h>
#include <DiaStreams/ServiceStreamStore.h>
#include <DiaStreams/IStreamConnector.h>

namespace Dia { namespace ApplicationFlow {

class Module;

// ServiceStreamReader<T>
// ---------------------------------------------------------------------------
// Handle held by a consuming Module for accessing a stable lifecycle handle
// from a ServiceStreamStore. Constructed by the module; call Connect(app)
// from the module's OnConnectStreams() override to wire the backing store.
//
// Get() is safe to call once IsAvailable() returns true. The framework
// guarantees that a consuming module's DoStart() is only called after the
// store has been committed, so a DoStart()-or-later call site may call
// Get() unconditionally (provided Connect() was called earlier).
// ---------------------------------------------------------------------------
template<typename T>
class ServiceStreamReader
{
public:
    ServiceStreamReader(Module* owner, const Dia::Core::StringCRC& streamId);

    // Called from the owning module's OnConnectStreams() override.
    void Connect(IStreamConnector& connector);

    // Returns the registered handle. Asserts that Connect() was called and
    // the store has been committed by the framework.
    T& Get() const;

    // True once the framework has committed the store (handle is accessible).
    bool IsAvailable()  const;
    bool IsConnected()  const;

private:
    Module*                mOwner;
    Dia::Core::StringCRC   mStreamId;
    ServiceStreamStore<T>* mStore = nullptr;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

template<typename T>
inline ServiceStreamReader<T>::ServiceStreamReader(Module* owner, const Dia::Core::StringCRC& streamId)
    : mOwner(owner)
    , mStreamId(streamId)
    , mStore(nullptr)
{
}

template<typename T>
inline void ServiceStreamReader<T>::Connect(IStreamConnector& connector)
{
    IStreamStore* istore = connector.RegisterOrFindStreamStore(
        Dia::Core::UniquePtr<IStreamStore>(
            new ServiceStreamStore<T>(mStreamId, Dia::Core::StringCRC::kZero)));
    mStore = static_cast<ServiceStreamStore<T>*>(istore);
}

template<typename T>
inline T& ServiceStreamReader<T>::Get() const
{
    DIA_ASSERT(mStore != nullptr,
        "ServiceStreamReader::Get — store is null; call Connect() from OnConnectStreams() first");
    return mStore->Get();
}

template<typename T>
inline bool ServiceStreamReader<T>::IsAvailable() const
{
    return mStore && mStore->IsCommitted();
}

template<typename T>
inline bool ServiceStreamReader<T>::IsConnected() const
{
    return mStore != nullptr;
}

}} // namespace Dia::ApplicationFlow
