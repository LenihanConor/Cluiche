#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaApplicationFlow/Streams/FrameStreamStore.h>
#include <DiaApplicationFlow/Application.h>

namespace Dia { namespace ApplicationFlow {

class Module;

// StreamWriter<T>
// ---------------------------------------------------------------------------
// Handle held by a Module for writing typed frame data to a FrameStreamStore.
// Constructed by the module; call Connect(app) from the module's
// OnConnectStreams() override to wire the backing store. IsConnected() returns
// false until Connect() is called.
// ---------------------------------------------------------------------------
template<typename T>
class StreamWriter
{
public:
    StreamWriter(Module* owner, const Dia::Core::StringCRC& streamId);

    void Write(const T& data, const Dia::Core::TimeAbsolute& timestamp);
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
inline StreamWriter<T>::StreamWriter(Module* owner, const Dia::Core::StringCRC& streamId)
    : mOwner(owner)
    , mStreamId(streamId)
    , mStore(nullptr)
{
}

template<typename T>
inline void StreamWriter<T>::Write(const T& data, const Dia::Core::TimeAbsolute& timestamp)
{
    if (mStore)
    {
        mStore->Write(data, timestamp);
    }
}

template<typename T>
inline bool StreamWriter<T>::IsConnected() const
{
    return mStore != nullptr;
}

template<typename T>
inline void StreamWriter<T>::Connect(Application& app)
{
    IStreamStore* istore = app.FindOrRegisterStreamStoreAtStartup(
        Dia::Core::UniquePtr<IStreamStore>(new FrameStreamStore<T>(mStreamId)));
    mStore = static_cast<FrameStreamStore<T>*>(istore);
}

}} // namespace Dia::ApplicationFlow
