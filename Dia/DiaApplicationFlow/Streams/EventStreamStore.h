#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cinttypes>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Log/DiaLog.h>

#include <DiaApplicationFlow/Streams/IStreamStore.h>
#include <DiaApplicationFlow/Streams/SendResult.h>
#include <DiaApplicationFlow/Streams/OverflowPolicy.h>
#include <DiaApplicationFlow/Streams/Event.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

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
//
// Frame-batching (service-channel F5):
//   Flush() advances mFlushSequence. Each event is stamped with the
//   mCurrentBatchId at Send() time. ConsumeUpToFlush() only drains events
//   whose batch stamp is <= mFlushSequence, providing frame-boundary
//   isolation. The existing Consume() is unchanged (drains all pending).
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

    // IStreamStore introspection overrides (F4)
    OverflowPolicy     GetOverflowPolicy()        const override { return mPolicy; }
    unsigned long long GetLastSequence()          const override
    {
        const uint64_t next = mNextSequence.load(std::memory_order_relaxed);
        return next > 0 ? static_cast<unsigned long long>(next - 1) : 0;
    }
    unsigned int       GetRegisteredReaderCount() const override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        unsigned int active = 0;
        for (int i = 0; i < mReaderCount; ++i)
            if (mReaders[i].active) ++active;
        return active;
    }

    // Called by framework to register a reader slot.
    // Returns reader index, or -1 if no slots remain.
    int RegisterReader();

    // Writer: fan-out event to all registered reader buffers.
    // Returns SendResult per overflow policy.
    SendResult Send(const Event<T>& event);

    // Frame-batching: advances the flush sequence, making all events sent
    // since the last Flush() visible to ConsumeUpToFlush().
    // Called by the framework at the end of the producer PU's tick.
    void Flush() override;

    // Frame-batching: drain only events whose batch stamp is <= the current
    // flush sequence. Events from the current (not-yet-flushed) batch are held.
    template<unsigned int N>
    void ConsumeUpToFlush(int readerIndex, Dia::Core::Containers::DynamicArrayC<Event<T>, N>& outEvents);

    // Returns the current flush sequence (number of Flush() calls made).
    uint64_t GetFlushSequence() const { return mFlushSequence.load(std::memory_order_acquire); }

    // Reader: drain ALL pending events (ignores flush boundary).
    template<unsigned int N>
    void Consume(int readerIndex, Dia::Core::Containers::DynamicArrayC<Event<T>, N>& outEvents);

    bool HasPending(int readerIndex) const;

    // Tap API (F4)
    TapHandle AttachTap(TapCallback cb) override;
    void      DetachTap(TapHandle handle) override;
    unsigned int GetTapCount() const override;

    // Shutdown notification — unblocks any kBlock writers waiting on condvar.
    void NotifyShutdown() override;

private:
    struct ReaderBuffer
    {
        Event<T>*    buffer       = nullptr;
        uint64_t*    batchStamps  = nullptr;  // parallel array: batch id for each slot
        unsigned int capacity     = 0;
        unsigned int head         = 0;
        unsigned int tail         = 0;
        unsigned int count        = 0;
        bool         active       = false;
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

    // Frame-batching: current batch id (incremented on each Flush) and the
    // last committed flush sequence visible to ConsumeUpToFlush().
    std::atomic<uint64_t>    mCurrentBatchId{1};   // starts at 1; 0 = pre-first-flush
    std::atomic<uint64_t>    mFlushSequence{0};    // 0 = no flush yet

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
        if (mReaders[i].buffer != nullptr)
        {
            delete[] mReaders[i].buffer;
            mReaders[i].buffer = nullptr;
        }
        if (mReaders[i].batchStamps != nullptr)
        {
            delete[] mReaders[i].batchStamps;
            mReaders[i].batchStamps = nullptr;
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
            mReaders[i].buffer      = new Event<T>[mCapacity];
            mReaders[i].batchStamps = new uint64_t[mCapacity]();
            mReaders[i].capacity    = mCapacity;
            mReaders[i].head        = 0;
            mReaders[i].tail        = 0;
            mReaders[i].count       = 0;
            mReaders[i].active      = true;
            if (i >= mReaderCount)
                mReaderCount = i + 1;
            DIA_LOG_INFO("stream", "stream.reader.connected stream_id=%s reader_index=%d", mId.AsChar(), i);
            return i;
        }
    }
    return -1;
}

template<typename T>
inline SendResult EventStreamStore<T>::Send(const Event<T>& event)
{
    DIA_PROFILE_SCOPE("stream.send", Dia::Observation::Profile::Category::kDiaStream);
    DIA_TRACE_ZONE("stream.send", Dia::Observation::Trace::Category::kDiaStream);
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
                    DIA_LOG_WARNING("stream",
                        "stream.overflow stream_id=%s reader=%d policy=kDropOldest",
                        mId.AsChar(), i);
                    break;

                case OverflowPolicy::kDropNewest:
                    result = SendResult::kDroppedNewest;
                    DIA_LOG_WARNING("stream",
                        "stream.overflow stream_id=%s reader=%d policy=kDropNewest",
                        mId.AsChar(), i);
                    continue;  // do not write for this reader

                case OverflowPolicy::kBlock:
                    // Already handled in Send() before entering SendInternal.
                    // If we get here the buffer is full despite the wait — drop oldest.
                    rb.tail = (rb.tail + 1) % rb.capacity;
                    --rb.count;
                    result = SendResult::kBlockedThenDropped;
                    DIA_LOG_WARNING("stream",
                        "stream.overflow stream_id=%s reader=%d policy=kBlock",
                        mId.AsChar(), i);
                    break;

                case OverflowPolicy::kFailLoud:
                    DIA_ASSERT(false,
                        "EventStreamStore '%s' overflow with fail-loud policy — reader %d buffer full",
                        mId.AsChar(), i);
                    result = SendResult::kFailLoudRejected;
                    DIA_LOG_WARNING("stream",
                        "stream.overflow stream_id=%s reader=%d policy=kFailLoudRejected",
                        mId.AsChar(), i);
                    return result;  // abort entire send
            }
        }

        rb.buffer[rb.head]      = event;
        rb.batchStamps[rb.head] = mCurrentBatchId.load(std::memory_order_relaxed);
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
    DIA_PROFILE_SCOPE("stream.consume", Dia::Observation::Profile::Category::kDiaStream);
    DIA_TRACE_ZONE("stream.consume", Dia::Observation::Trace::Category::kDiaStream);
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

// --- Frame-batching API ------------------------------------------------------

template<typename T>
inline void EventStreamStore<T>::Flush()
{
    // Commit the current batch: advance flush sequence to match current batch id,
    // then advance current batch id so new events belong to the next batch.
    const uint64_t committed = mCurrentBatchId.load(std::memory_order_relaxed);
    mFlushSequence.store(committed, std::memory_order_release);
    mCurrentBatchId.fetch_add(1, std::memory_order_relaxed);
    DIA_LOG_DEBUG("stream", "stream.flush stream_id=%s flush_seq=%" PRIu64, mId.AsChar(), committed);
}

template<typename T>
template<unsigned int N>
inline void EventStreamStore<T>::ConsumeUpToFlush(int readerIndex,
    Dia::Core::Containers::DynamicArrayC<Event<T>, N>& outEvents)
{
    DIA_ASSERT(readerIndex >= 0 && readerIndex < mReaderCount,
        "EventStreamStore::ConsumeUpToFlush — invalid readerIndex %d", readerIndex);

    const uint64_t flushedSeq = mFlushSequence.load(std::memory_order_acquire);

    std::lock_guard<std::mutex> lock(mMutex);
    ReaderBuffer& rb = mReaders[readerIndex];
    if (!rb.active)
        return;

    while (rb.count > 0 && !outEvents.IsFull())
    {
        // Peek at the batch stamp of the oldest event
        const uint64_t stamp = rb.batchStamps[rb.tail];
        if (stamp > flushedSeq)
            break;  // event belongs to an unflushed batch — stop here

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
            DIA_LOG_DEBUG("stream", "stream.tap.attached stream_id=%s tap_id=%u", mId.AsChar(), h.id);
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
            DIA_LOG_DEBUG("stream", "stream.tap.detached stream_id=%s tap_id=%u", mId.AsChar(), handle.id);
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
