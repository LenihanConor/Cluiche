#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstdint>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaLogger/DiaLog.h>

#include <DiaApplicationFlow/Streams/IStreamStore.h>
#include <DiaApplicationFlow/Streams/SendResult.h>
#include <DiaApplicationFlow/Streams/OverflowPolicy.h>
#include <DiaApplicationFlow/Streams/Event.h>

namespace Dia { namespace ApplicationFlow {

// TapCallback and TapHandle are defined in IStreamStore.h (included above).

// EventStreamStore<T>
// ---------------------------------------------------------------------------
// Internal store owned by Application. Maintains a per-reader fixed ring
// buffer for discrete event fan-out. Each registered reader gets an
// independent copy of every sent event, wrapped in an Event<T> envelope.
//
// Overflow behaviour is controlled by the OverflowPolicy set at construction.
// Default policy: kDropOldest (matches pre-F3 behaviour).
// ---------------------------------------------------------------------------
template<typename T>
class EventStreamStore : public IStreamStore
{
public:
    static constexpr unsigned int kDefaultCapacity   = 256;
    static constexpr unsigned int kDefaultMaxReaders = 8;
    static constexpr unsigned int kMaxTaps           = 8;

    explicit EventStreamStore(const Dia::Core::StringCRC& id,
                              const Dia::Core::StringCRC& payloadType = Dia::Core::StringCRC::kZero,
                              unsigned int capacity    = kDefaultCapacity,
                              unsigned int maxReaders  = kDefaultMaxReaders,
                              OverflowPolicy policy    = OverflowPolicy::kDropOldest,
                              unsigned int blockTimeoutMs = 100);
    ~EventStreamStore();

    EventStreamStore(const EventStreamStore&)            = delete;
    EventStreamStore& operator=(const EventStreamStore&) = delete;

    const Dia::Core::StringCRC& GetId()          const override;
    StreamKind                  GetKind()         const override { return StreamKind::kEvent; }
    const Dia::Core::StringCRC& GetPayloadType()  const override { return mPayloadType; }
    unsigned int                GetMaxReaders()   const override { return mMaxReaders; }

    // Called by framework to register a reader slot.
    // Returns reader index, or -1 if no slots remain.
    int RegisterReader();

    // Writer: fan-out event to all registered reader buffers.
    // Returns SendResult per overflow policy.
    SendResult Send(const Event<T>& event);

    // Reader: drain events from reader[readerIndex] into outEvents (up to N).
    template<unsigned int N>
    void Consume(int readerIndex, Dia::Core::Containers::DynamicArrayC<Event<T>, N>& outEvents);

    bool HasPending(int readerIndex) const;

    // Tap API (F4)
    TapHandle AttachTap(TapCallback cb);
    void      DetachTap(TapHandle handle);
    unsigned int GetTapCount() const;

    // Shutdown notification — unblocks any kBlock writers waiting on condvar.
    void NotifyShutdown();

private:
    struct ReaderBuffer
    {
        Event<T>*    buffer   = nullptr;
        unsigned int capacity = 0;
        unsigned int head     = 0;
        unsigned int tail     = 0;
        unsigned int count    = 0;
        bool         active   = false;
    };

    struct TapEntry
    {
        TapCallback  callback;
        unsigned int id = 0;
    };

    SendResult SendInternal(const Event<T>& event);
    void DispatchTaps(const Event<T>& event);

    Dia::Core::StringCRC mId;
    Dia::Core::StringCRC mPayloadType;
    unsigned int         mCapacity;
    unsigned int         mMaxReaders;
    OverflowPolicy       mPolicy;
    unsigned int         mBlockTimeoutMs;

    mutable std::mutex       mMutex;
    std::condition_variable  mNotFullCv;
    std::atomic<bool>        mShuttingDown{false};
    std::atomic<uint64_t>    mNextSequence{0};

    ReaderBuffer mReaders[kDefaultMaxReaders];
    int          mReaderCount = 0;

    TapEntry     mTaps[kMaxTaps];
    unsigned int mTapCount     = 0;
    unsigned int mNextTapId    = 1;
    bool         mInDispatch   = false;  // re-entrance guard
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

template<typename T>
inline EventStreamStore<T>::EventStreamStore(const Dia::Core::StringCRC& id,
                                              const Dia::Core::StringCRC& payloadType,
                                              unsigned int capacity,
                                              unsigned int maxReaders,
                                              OverflowPolicy policy,
                                              unsigned int blockTimeoutMs)
    : mId(id)
    , mPayloadType(payloadType)
    , mCapacity(capacity)
    , mMaxReaders(maxReaders)
    , mPolicy(policy)
    , mBlockTimeoutMs(blockTimeoutMs)
    , mReaderCount(0)
    , mTapCount(0)
    , mNextTapId(1)
    , mInDispatch(false)
{
}

template<typename T>
inline EventStreamStore<T>::~EventStreamStore()
{
    for (int i = 0; i < static_cast<int>(kDefaultMaxReaders); ++i)
    {
        if (mReaders[i].active && mReaders[i].buffer != nullptr)
        {
            delete[] mReaders[i].buffer;
            mReaders[i].buffer = nullptr;
        }
    }
}

template<typename T>
inline const Dia::Core::StringCRC& EventStreamStore<T>::GetId() const
{
    return mId;
}

template<typename T>
inline int EventStreamStore<T>::RegisterReader()
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mReaderCount >= static_cast<int>(mMaxReaders))
        return -1;
    for (int i = 0; i < static_cast<int>(kDefaultMaxReaders); ++i)
    {
        if (!mReaders[i].active)
        {
            mReaders[i].buffer   = new Event<T>[mCapacity];
            mReaders[i].capacity = mCapacity;
            mReaders[i].head     = 0;
            mReaders[i].tail     = 0;
            mReaders[i].count    = 0;
            mReaders[i].active   = true;
            if (i >= mReaderCount)
                mReaderCount = i + 1;
            return i;
        }
    }
    return -1;
}

template<typename T>
inline SendResult EventStreamStore<T>::Send(const Event<T>& event)
{
    if (mPolicy == OverflowPolicy::kBlock)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        // Wait until space is available in ALL reader buffers, or timeout/shutdown.
        bool timedOut = !mNotFullCv.wait_for(lock,
            std::chrono::milliseconds(mBlockTimeoutMs),
            [this]() -> bool {
                if (mShuttingDown.load(std::memory_order_relaxed))
                    return true;
                for (int i = 0; i < mReaderCount; ++i)
                {
                    if (mReaders[i].active && mReaders[i].count >= mReaders[i].capacity)
                        return false;
                }
                return true;
            });

        if (mShuttingDown.load(std::memory_order_relaxed))
            return SendResult::kDroppedOldest;  // shutdown drain, discard silently

        if (timedOut)
        {
            // Timeout expired — fall through to drop-oldest for each blocked reader.
            SendResult r = SendInternal(event);
            mNotFullCv.notify_all();
            return r == SendResult::kDroppedOldest ? SendResult::kBlockedThenDropped : r;
        }

        SendResult r = SendInternal(event);
        mNotFullCv.notify_all();
        return r;
    }
    else
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return SendInternal(event);
    }
}

template<typename T>
inline SendResult EventStreamStore<T>::SendInternal(const Event<T>& event)
{
    // Re-entrance guard: tap callbacks must not call Send() on this store.
    DIA_ASSERT(!mInDispatch,
        "EventStreamStore::Send called re-entrantly from a TapCallback — forbidden");

    // Stamp the per-store sequence number now (inside the lock).
    const_cast<Event<T>&>(event).sequence = mNextSequence.fetch_add(1, std::memory_order_relaxed);

    SendResult result = SendResult::kDelivered;

    for (int i = 0; i < mReaderCount; ++i)
    {
        ReaderBuffer& rb = mReaders[i];
        if (!rb.active)
            continue;

        if (rb.count >= rb.capacity)
        {
            switch (mPolicy)
            {
                case OverflowPolicy::kDropOldest:
                    rb.tail = (rb.tail + 1) % rb.capacity;
                    --rb.count;
                    result = SendResult::kDroppedOldest;
                    DIA_LOG_WARNING("EventStream",
                        "EventStreamStore overflow on '%s' reader %d — oldest dropped",
                        mId.AsChar(), i);
                    break;

                case OverflowPolicy::kDropNewest:
                    result = SendResult::kDroppedNewest;
                    continue;  // do not write for this reader

                case OverflowPolicy::kBlock:
                    // Already handled in Send() before entering SendInternal.
                    // If we get here the buffer is full despite the wait — drop oldest.
                    rb.tail = (rb.tail + 1) % rb.capacity;
                    --rb.count;
                    result = SendResult::kBlockedThenDropped;
                    break;

                case OverflowPolicy::kFailLoud:
                    DIA_ASSERT(false,
                        "EventStreamStore '%s' overflow with fail-loud policy — reader %d buffer full",
                        mId.AsChar(), i);
                    result = SendResult::kFailLoudRejected;
                    return result;  // abort entire send
            }
        }

        rb.buffer[rb.head] = event;
        rb.head = (rb.head + 1) % rb.capacity;
        ++rb.count;
    }

    // Dispatch taps after reader fan-out, before condvar notify (F4).
    DispatchTaps(event);

    return result;
}

template<typename T>
inline void EventStreamStore<T>::DispatchTaps(const Event<T>& event)
{
    if (mTapCount == 0)
        return;

    mInDispatch = true;
    const unsigned int count = mTapCount;
    for (unsigned int i = 0; i < count; ++i)
    {
        if (mTaps[i].id != 0 && mTaps[i].callback)
        {
            mTaps[i].callback(
                static_cast<const void*>(&event.payload),
                static_cast<unsigned int>(sizeof(T)),
                mId);
        }
    }
    mInDispatch = false;
}

template<typename T>
template<unsigned int N>
inline void EventStreamStore<T>::Consume(int readerIndex,
    Dia::Core::Containers::DynamicArrayC<Event<T>, N>& outEvents)
{
    DIA_ASSERT(readerIndex >= 0 && readerIndex < mReaderCount,
        "EventStreamStore::Consume — invalid readerIndex %d", readerIndex);

    std::lock_guard<std::mutex> lock(mMutex);
    ReaderBuffer& rb = mReaders[readerIndex];
    if (!rb.active)
        return;

    while (rb.count > 0 && !outEvents.IsFull())
    {
        outEvents.Add(rb.buffer[rb.tail]);
        rb.tail = (rb.tail + 1) % rb.capacity;
        --rb.count;
    }

    if (mPolicy == OverflowPolicy::kBlock)
        mNotFullCv.notify_all();
}

template<typename T>
inline bool EventStreamStore<T>::HasPending(int readerIndex) const
{
    DIA_ASSERT(readerIndex >= 0 && readerIndex < mReaderCount,
        "EventStreamStore::HasPending — invalid readerIndex %d", readerIndex);

    std::lock_guard<std::mutex> lock(mMutex);
    return mReaders[readerIndex].active && mReaders[readerIndex].count > 0;
}

template<typename T>
inline void EventStreamStore<T>::NotifyShutdown()
{
    mShuttingDown.store(true, std::memory_order_release);
    mNotFullCv.notify_all();
}

// --- Tap API -----------------------------------------------------------------

template<typename T>
inline TapHandle EventStreamStore<T>::AttachTap(TapCallback cb)
{
    std::lock_guard<std::mutex> lock(mMutex);
    DIA_ASSERT(mTapCount < kMaxTaps,
        "EventStreamStore '%s' AttachTap — max taps (%u) reached", mId.AsChar(), kMaxTaps);
    if (mTapCount >= kMaxTaps)
        return TapHandle{0};

    TapHandle h{ mNextTapId++ };
    for (unsigned int i = 0; i < kMaxTaps; ++i)
    {
        if (mTaps[i].id == 0)
        {
            mTaps[i].callback = std::move(cb);
            mTaps[i].id       = h.id;
            ++mTapCount;
            return h;
        }
    }
    return TapHandle{0};
}

template<typename T>
inline void EventStreamStore<T>::DetachTap(TapHandle handle)
{
    if (handle.id == 0)
        return;
    std::lock_guard<std::mutex> lock(mMutex);
    for (unsigned int i = 0; i < kMaxTaps; ++i)
    {
        if (mTaps[i].id == handle.id)
        {
            mTaps[i].callback = nullptr;
            mTaps[i].id       = 0;
            --mTapCount;
            return;
        }
    }
}

template<typename T>
inline unsigned int EventStreamStore<T>::GetTapCount() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mTapCount;
}

}} // namespace Dia::ApplicationFlow
