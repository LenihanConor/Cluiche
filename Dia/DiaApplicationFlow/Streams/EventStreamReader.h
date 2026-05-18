#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Streams/Event.h>
#include <DiaApplicationFlow/Application.h>

namespace Dia { namespace ApplicationFlow {

class Module;

// EventStreamReader<T>
// ---------------------------------------------------------------------------
// Handle held by a Module for consuming discrete typed events from an
// EventStreamStore. Constructed by the module; call Connect(app) from the
// module's OnConnectStreams() override to wire the backing store and register
// a per-reader slot.
//
// Consume() drains into DynamicArrayC<Event<T>, N>. Each event carries the
// full envelope (timestampUs, senderCrc, sequence, payload).  Access the
// payload via ev.payload.
// ---------------------------------------------------------------------------
template<typename T>
class EventStreamReader
{
public:
    EventStreamReader(Module* owner, const Dia::Core::StringCRC& streamId);

    template<unsigned int N>
    void Consume(Dia::Core::Containers::DynamicArrayC<Event<T>, N>& outEvents);

    bool HasPending() const;
    bool IsConnected() const;

    // Called from the owning module's OnConnectStreams() override.
    void Connect(Application& app);

private:
    Module*               mOwner;
    Dia::Core::StringCRC  mStreamId;
    EventStreamStore<T>*  mStore       = nullptr;
    int                   mReaderIndex = -1;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

template<typename T>
inline EventStreamReader<T>::EventStreamReader(Module* owner, const Dia::Core::StringCRC& streamId)
    : mOwner(owner)
    , mStreamId(streamId)
    , mStore(nullptr)
    , mReaderIndex(-1)
{
}

template<typename T>
template<unsigned int N>
inline void EventStreamReader<T>::Consume(Dia::Core::Containers::DynamicArrayC<Event<T>, N>& outEvents)
{
    if (mStore && mReaderIndex >= 0)
    {
        mStore->Consume(mReaderIndex, outEvents);
    }
}

template<typename T>
inline bool EventStreamReader<T>::HasPending() const
{
    return mStore && mReaderIndex >= 0 && mStore->HasPending(mReaderIndex);
}

template<typename T>
inline bool EventStreamReader<T>::IsConnected() const
{
    return mStore != nullptr && mReaderIndex >= 0;
}

template<typename T>
inline void EventStreamReader<T>::Connect(Application& app)
{
    IStreamStore* istore = app.RegisterOrFindStreamStore(
        Dia::Core::UniquePtr<IStreamStore>(new EventStreamStore<T>(mStreamId)));
    if (istore)
    {
        mStore       = static_cast<EventStreamStore<T>*>(istore);
        mReaderIndex = mStore->RegisterReader();
    }
}

}} // namespace Dia::ApplicationFlow
