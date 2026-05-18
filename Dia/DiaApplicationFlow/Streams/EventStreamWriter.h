#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Streams/SendResult.h>
#include <DiaApplicationFlow/Application.h>

namespace Dia { namespace ApplicationFlow {

class Module;

// EventStreamWriter<T>
// ---------------------------------------------------------------------------
// Handle held by a Module for sending discrete typed events into an
// EventStreamStore. Constructed by the module; call Connect(app) from the
// module's OnConnectStreams() override to wire the backing store.
//
// Send() stamps the Event<T> envelope (timestamp, senderCrc, sequence) and
// returns SendResult. Callers that don't need to distinguish outcomes can
// treat (result != kFailLoudRejected) as success.
// ---------------------------------------------------------------------------
template<typename T>
class EventStreamWriter
{
public:
    EventStreamWriter(Module* owner, const Dia::Core::StringCRC& streamId);

    SendResult Send(const T& payload);
    bool IsConnected() const;

    // Called from the owning module's OnConnectStreams() override.
    void Connect(Application& app);

    // Framework-internal: directly wire to an already-created store.
    // Used by Application to wire the $lifecycle writer without going through
    // the manifest-gated RegisterOrFindStreamStore path.
    void ConnectToStore(EventStreamStore<T>* store) { mStore = store; }

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
inline SendResult EventStreamWriter<T>::Send(const T& payload)
{
    if (!mStore)
        return SendResult::kFailLoudRejected;

    static const Dia::Core::StringCRC kFrameworkId("$framework");

    Event<T> ev;
    ev.timestampUs = Dia::Core::TimeAbsolute::GetSystemTime().AsLongLongInMicroseconds();
    ev.senderCrc   = mOwner ? mOwner->GetInstanceId().Value() : kFrameworkId.Value();
    ev.sequence    = 0;  // per-store sequence stamped inside EventStreamStore::Send
    ev.payload     = payload;

    return mStore->Send(ev);
}

template<typename T>
inline bool EventStreamWriter<T>::IsConnected() const
{
    return mStore != nullptr;
}

template<typename T>
inline void EventStreamWriter<T>::Connect(Application& app)
{
    IStreamStore* istore = app.RegisterOrFindStreamStore(
        Dia::Core::UniquePtr<IStreamStore>(new EventStreamStore<T>(mStreamId)));
    mStore = static_cast<EventStreamStore<T>*>(istore);
}

}} // namespace Dia::ApplicationFlow
