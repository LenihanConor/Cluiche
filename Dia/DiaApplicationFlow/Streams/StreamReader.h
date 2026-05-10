#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaApplicationFlow/Streams/FrameStreamStore.h>
#include <DiaApplicationFlow/Application.h>

namespace Dia { namespace ApplicationFlow {

class Module;

// StreamReader<T>
// ---------------------------------------------------------------------------
// Handle held by a Module for reading typed frame data from a FrameStreamStore.
// Constructed by the module; call Connect(app) from the module's
// OnConnectStreams() override to wire the backing store. IsConnected() returns
// false until Connect() is called.
// ---------------------------------------------------------------------------
template<typename T>
class StreamReader
{
public:
    StreamReader(Module* owner, const Dia::Core::StringCRC& streamId);

    const T* FetchLatest() const;
    // FetchClosestTo: returns latest; ring-buffer interpolation is future work
    const T* FetchClosestTo(const Dia::Core::TimeAbsolute& time) const;
    bool IsConnected() const;

    // Called from the owning module's OnConnectStreams() override.
    // Finds or creates the FrameStreamStore<T> with this ID in app.
    void Connect(Application& app);

private:
    Module*               mOwner;
    Dia::Core::StringCRC  mStreamId;
    FrameStreamStore<T>*  mStore = nullptr;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

template<typename T>
inline StreamReader<T>::StreamReader(Module* owner, const Dia::Core::StringCRC& streamId)
    : mOwner(owner)
    , mStreamId(streamId)
    , mStore(nullptr)
{
}

template<typename T>
inline const T* StreamReader<T>::FetchLatest() const
{
    return mStore ? mStore->FetchLatest() : nullptr;
}

template<typename T>
inline const T* StreamReader<T>::FetchClosestTo(const Dia::Core::TimeAbsolute& time) const
{
    return mStore ? mStore->FetchClosestTo(time) : nullptr;
}

template<typename T>
inline bool StreamReader<T>::IsConnected() const
{
    return mStore != nullptr;
}

template<typename T>
inline void StreamReader<T>::Connect(Application& app)
{
    IStreamStore* istore = app.FindOrRegisterStreamStoreAtStartup(
        Dia::Core::UniquePtr<IStreamStore>(new FrameStreamStore<T>(mStreamId)));
    mStore = static_cast<FrameStreamStore<T>*>(istore);
}

}} // namespace Dia::ApplicationFlow
