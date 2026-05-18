#pragma once

#include <atomic>
#include <mutex>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaApplicationFlow/Streams/IStreamStore.h>

namespace Dia { namespace ApplicationFlow {

// FrameStreamStore<T>
// ---------------------------------------------------------------------------
// Internal store owned by Application. Holds the "latest frame" for a typed
// data channel. Readers are lock-free for the single-writer case; multi-writer
// mode opts into a mutex-protected write.
//
// TimeAbsolute has no public default constructor, so timestamps are stored as
// raw int64 microsecond values. The public Write() API accepts TimeAbsolute.
// ---------------------------------------------------------------------------
template<typename T>
class FrameStreamStore : public IStreamStore
{
public:
    explicit FrameStreamStore(const Dia::Core::StringCRC& id,
                              const Dia::Core::StringCRC& payloadType = Dia::Core::StringCRC::kZero,
                              bool multiWriter = false);

    // Writer side
    void Write(const T& data, const Dia::Core::TimeAbsolute& timestamp);

    // Reader side (lock-free for single-writer)
    const T* FetchLatest() const;
    // FetchClosestTo: returns latest; ring-buffer interpolation is future work
    const T* FetchClosestTo(const Dia::Core::TimeAbsolute& time) const;

    const Dia::Core::StringCRC& GetId() const override;
    StreamKind GetKind() const override { return StreamKind::kFrame; }
    const Dia::Core::StringCRC& GetPayloadType() const override { return mPayloadType; }
    unsigned int GetMaxReaders() const override { return 0; }

private:
    // Double buffer: two slots, atomic index selects which is "front"
    struct Slot
    {
        T        data{};
        long long timestampUs = 0LL;  // raw microseconds; TimeAbsolute has no default ctor
        bool     hasData      = false;
    };

    Dia::Core::StringCRC  mId;
    Dia::Core::StringCRC  mPayloadType;
    bool                  mMultiWriter;
    Slot                  mSlots[2];
    std::atomic<int>      mFrontIndex{0};    // readers read mSlots[mFrontIndex]
    mutable std::mutex    mWriteMutex;       // only used when mMultiWriter == true
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

template<typename T>
inline FrameStreamStore<T>::FrameStreamStore(const Dia::Core::StringCRC& id,
                                              const Dia::Core::StringCRC& payloadType,
                                              bool multiWriter)
    : mId(id)
    , mPayloadType(payloadType)
    , mMultiWriter(multiWriter)
    , mFrontIndex(0)
{
}

template<typename T>
inline void FrameStreamStore<T>::Write(const T& data, const Dia::Core::TimeAbsolute& timestamp)
{
    if (mMultiWriter)
    {
        std::lock_guard<std::mutex> lock(mWriteMutex);
        int front = mFrontIndex.load(std::memory_order_relaxed);
        int back  = 1 - front;
        mSlots[back].data        = data;
        mSlots[back].timestampUs = timestamp.AsLongLongInMicroseconds();
        mSlots[back].hasData     = true;
        mFrontIndex.store(back, std::memory_order_release);
    }
    else
    {
        int front = mFrontIndex.load(std::memory_order_relaxed);
        int back  = 1 - front;
        mSlots[back].data        = data;
        mSlots[back].timestampUs = timestamp.AsLongLongInMicroseconds();
        mSlots[back].hasData     = true;
        mFrontIndex.store(back, std::memory_order_release);
    }
}

template<typename T>
inline const T* FrameStreamStore<T>::FetchLatest() const
{
    int i = mFrontIndex.load(std::memory_order_acquire);
    return mSlots[i].hasData ? &mSlots[i].data : nullptr;
}

template<typename T>
inline const T* FrameStreamStore<T>::FetchClosestTo(const Dia::Core::TimeAbsolute& /*time*/) const
{
    // Ring-buffer interpolation is future work; return latest for now.
    return FetchLatest();
}

template<typename T>
inline const Dia::Core::StringCRC& FrameStreamStore<T>::GetId() const
{
    return mId;
}

}} // namespace Dia::ApplicationFlow
