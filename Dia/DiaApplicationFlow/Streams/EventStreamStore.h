#pragma once

#include <mutex>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>
#include <DiaApplicationFlow/Streams/IStreamStore.h>

namespace Dia { namespace ApplicationFlow {

// EventStreamStore<T>
// ---------------------------------------------------------------------------
// Internal store owned by Application. Maintains a per-reader fixed ring
// buffer for discrete event fan-out. Each registered reader gets an
// independent copy of every sent event. Overflow drops the oldest event and
// logs a warning.
// ---------------------------------------------------------------------------
template<typename T>
class EventStreamStore : public IStreamStore
{
public:
    static constexpr unsigned int kDefaultCapacity = 256;
    static constexpr unsigned int kMaxReaders      = 8;

    explicit EventStreamStore(const Dia::Core::StringCRC& id,
                              unsigned int capacity = kDefaultCapacity);
    ~EventStreamStore();

    // No copy or move — stores are owned by Application
    EventStreamStore(const EventStreamStore&)            = delete;
    EventStreamStore& operator=(const EventStreamStore&) = delete;

    const Dia::Core::StringCRC& GetId() const override;
    StreamKind GetKind() const override { return StreamKind::kEvent; }

    // Called by framework to register a reader slot.
    // Returns reader index, or -1 if no slots remain.
    int RegisterReader();

    // Writer: fans the event out to all registered reader buffers
    void Send(const T& event);

    // Reader: drains events from reader[readerIndex] into outArray (up to N)
    template<unsigned int N>
    void Consume(int readerIndex, Dia::Core::Containers::DynamicArrayC<T, N>& outEvents);

    // Returns true if reader[readerIndex] has pending events
    bool HasPending(int readerIndex) const;

private:
    // Per-reader ring buffer (heap-allocated, fixed capacity)
    struct ReaderBuffer
    {
        T*           buffer   = nullptr;
        unsigned int capacity = 0;
        unsigned int head     = 0;    // next write position
        unsigned int tail     = 0;    // next read position
        unsigned int count    = 0;
        bool         active   = false;
    };

    Dia::Core::StringCRC mId;
    unsigned int         mCapacity;
    mutable std::mutex   mMutex;
    ReaderBuffer         mReaders[kMaxReaders];
    int                  mReaderCount = 0;
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

template<typename T>
inline EventStreamStore<T>::EventStreamStore(const Dia::Core::StringCRC& id,
                                              unsigned int capacity)
    : mId(id)
    , mCapacity(capacity)
    , mReaderCount(0)
{
}

template<typename T>
inline EventStreamStore<T>::~EventStreamStore()
{
    for (int i = 0; i < kMaxReaders; ++i)
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
    for (int i = 0; i < static_cast<int>(kMaxReaders); ++i)
    {
        if (!mReaders[i].active)
        {
            mReaders[i].buffer   = new T[mCapacity];
            mReaders[i].capacity = mCapacity;
            mReaders[i].head     = 0;
            mReaders[i].tail     = 0;
            mReaders[i].count    = 0;
            mReaders[i].active   = true;
            if (i >= mReaderCount)
            {
                mReaderCount = i + 1;
            }
            return i;
        }
    }
    return -1; // no slots available
}

template<typename T>
inline void EventStreamStore<T>::Send(const T& event)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (int i = 0; i < mReaderCount; ++i)
    {
        ReaderBuffer& rb = mReaders[i];
        if (!rb.active)
            continue;

        if (rb.count == rb.capacity)
        {
            // Drop oldest to make room
            rb.tail = (rb.tail + 1) % rb.capacity;
            --rb.count;
            DIA_LOG_WARNING("EventStream", "EventStreamStore overflow on stream '%s' reader %d — oldest event dropped",
                            mId.AsChar(), i);
        }

        rb.buffer[rb.head] = event;
        rb.head            = (rb.head + 1) % rb.capacity;
        ++rb.count;
    }
}

template<typename T>
template<unsigned int N>
inline void EventStreamStore<T>::Consume(int readerIndex, Dia::Core::Containers::DynamicArrayC<T, N>& outEvents)
{
    DIA_ASSERT(readerIndex >= 0 && readerIndex < mReaderCount, "EventStreamStore::Consume — invalid readerIndex %d", readerIndex);

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
}

template<typename T>
inline bool EventStreamStore<T>::HasPending(int readerIndex) const
{
    DIA_ASSERT(readerIndex >= 0 && readerIndex < mReaderCount, "EventStreamStore::HasPending — invalid readerIndex %d", readerIndex);

    std::lock_guard<std::mutex> lock(mMutex);
    return mReaders[readerIndex].active && mReaders[readerIndex].count > 0;
}

}} // namespace Dia::ApplicationFlow
