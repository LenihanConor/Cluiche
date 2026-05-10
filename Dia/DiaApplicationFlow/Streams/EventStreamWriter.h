#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Application.h>

namespace Dia { namespace ApplicationFlow {

class Module;

// EventStreamWriter<T>
// ---------------------------------------------------------------------------
// Handle held by a Module for sending discrete typed events into an
// EventStreamStore. Constructed by the module; call Connect(app) from the
// module's OnConnectStreams() override to wire the backing store.
// IsConnected() returns false until Connect() is called.
// ---------------------------------------------------------------------------
template<typename T>
class EventStreamWriter
{
public:
    EventStreamWriter(Module* owner, const Dia::Core::StringCRC& streamId);

    void Send(const T& event);
    bool IsConnected() const;

    // Called from the owning module's OnConnectStreams() override.
    // Finds or creates the EventStreamStore<T> with this ID in app.
    void Connect(Application& app);

private:
    Module*               mOwner;
    Dia::Core::StringCRC  mStreamId;
    EventStreamStore<T>*  mStore = nullptr;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

template<typename T>
inline EventStreamWriter<T>::EventStreamWriter(Module* owner, const Dia::Core::StringCRC& streamId)
    : mOwner(owner)
    , mStreamId(streamId)
    , mStore(nullptr)
{
}

template<typename T>
inline void EventStreamWriter<T>::Send(const T& event)
{
    if (mStore)
    {
        mStore->Send(event);
    }
}

template<typename T>
inline bool EventStreamWriter<T>::IsConnected() const
{
    return mStore != nullptr;
}

template<typename T>
inline void EventStreamWriter<T>::Connect(Application& app)
{
    IStreamStore* istore = app.FindOrRegisterStreamStore(
        Dia::Core::UniquePtr<IStreamStore>(new EventStreamStore<T>(mStreamId)));
    mStore = static_cast<EventStreamStore<T>*>(istore);
}

}} // namespace Dia::ApplicationFlow
