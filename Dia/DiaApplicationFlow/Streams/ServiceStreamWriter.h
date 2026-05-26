#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/Core/Assert.h>
#include <DiaApplicationFlow/Streams/ServiceStreamStore.h>
#include <DiaApplicationFlow/Application.h>

namespace Dia { namespace ApplicationFlow {

class Module;

// ServiceStreamWriter<T>
// ---------------------------------------------------------------------------
// Handle held by a providing Module for registering a stable lifecycle handle
// into a ServiceStreamStore. Constructed by the module; call Connect(app)
// from the module's OnConnectStreams() override to wire the backing store,
// then call Register(handle) from DoStart() once the handle is ready.
//
// The framework calls Commit() on the store after all providers in scope
// have registered — consumers may call Get() only after that point.
// ---------------------------------------------------------------------------
template<typename T>
class ServiceStreamWriter
{
public:
    ServiceStreamWriter(Module* owner, const Dia::Core::StringCRC& streamId);

    // Called from the owning module's OnConnectStreams() override.
    void Connect(Application& app);

    // Called from the owning module's DoStart() once the handle is ready.
    // Asserts that Connect() was called first.
    void Register(T& handle);

    bool IsConnected()  const;
    bool IsRegistered() const;

private:
    Module*                mOwner;
    Dia::Core::StringCRC   mStreamId;
    ServiceStreamStore<T>* mStore = nullptr;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

template<typename T>
inline ServiceStreamWriter<T>::ServiceStreamWriter(Module* owner, const Dia::Core::StringCRC& streamId)
    : mOwner(owner)
    , mStreamId(streamId)
    , mStore(nullptr)
{
}

template<typename T>
inline void ServiceStreamWriter<T>::Connect(Application& app)
{
    IStreamStore* istore = app.RegisterOrFindStreamStore(
        Dia::Core::UniquePtr<IStreamStore>(
            new ServiceStreamStore<T>(mStreamId, Dia::Core::StringCRC::kZero)));
    mStore = static_cast<ServiceStreamStore<T>*>(istore);
}

template<typename T>
inline void ServiceStreamWriter<T>::Register(T& handle)
{
    DIA_ASSERT(mStore != nullptr,
        "ServiceStreamWriter::Register — store is null; call Connect() from OnConnectStreams() first");
    mStore->Register(handle);
}

template<typename T>
inline bool ServiceStreamWriter<T>::IsConnected() const
{
    return mStore != nullptr;
}

template<typename T>
inline bool ServiceStreamWriter<T>::IsRegistered() const
{
    return mStore && mStore->IsRegistered();
}

}} // namespace Dia::ApplicationFlow
