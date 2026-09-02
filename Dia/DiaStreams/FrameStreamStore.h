#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaStreams/IStreamStore.h>
#include <DiaStreams/FrameStreamDiagnostics.h>

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
    // FetchClosestTo: returns a pointer to the ALREADY-STORED sample whose
    // timestamp is nearest the requested time (nearest-sample selection, not a
    // blended/interpolated value — the const T* signature cannot return a
    // synthesized temporary).
    const T* FetchClosestTo(const Dia::Core::TimeAbsolute& time) const;

    const Dia::Core::StringCRC& GetId() const override;
    StreamKind GetKind() const override { return StreamKind::kFrame; }
    const Dia::Core::StringCRC& GetPayloadType() const override { return mPayloadType; }
    unsigned int GetMaxReaders() const override { return 0; }

private:
    // N-slot ring buffer. The writer walks mWriteCursor forward through the ring;
    // readers load mWriteCursor and read backward from it. Several frames of
    // interpolation/lookback headroom.
    static constexpr unsigned int kRingCapacity = 8;

    struct Slot
    {
        // std::optional<T> rather than a default-constructed T: some payload types
        // (e.g. SimTimeContext) have members with only private/factory constructors
        // (TimeAbsolute/TimeRelative) and are therefore not default-constructible.
        // optional<T> defaults to empty with no requirement on T's default ctor.
        std::optional<T> data;
        long long timestampUs = 0LL;  // raw microseconds; TimeAbsolute has no default ctor
        bool     hasData      = false;
    };

    // Concurrency tradeoff: single-writer reads are lock-free. A reader captures
    // mWriteCursor then scans up to kRingCapacity slots backward. There is a
    // theoretical race only if the writer completes kRingCapacity more Write()
    // calls (wrapping the whole ring) before the reader finishes its 8-slot scan.
    // This is the same class of risk the original 2-slot double buffer had, just
    // with a larger safety margin (N=8 vs N=2), and is accepted as negligible for
    // this codebase's per-tick call rate. Do NOT add ABA protection / versioned
    // slots / hazard pointers here — out of scope for these concurrency needs.
    Dia::Core::StringCRC  mId;
    Dia::Core::StringCRC  mPayloadType;
    bool                  mMultiWriter;
    Slot                  mSlots[kRingCapacity];
    std::atomic<int>      mWriteCursor{-1};  // index of most recently written slot; -1 = never written
    mutable std::mutex    mWriteMutex;       // only used when mMultiWriter == true
    mutable std::atomic<bool> mWarnedNoData{false};  // fire-once warn when a Fetch returns nullptr
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
    , mWriteCursor(-1)
{
}

template<typename T>
inline void FrameStreamStore<T>::Write(const T& data, const Dia::Core::TimeAbsolute& timestamp)
{
    mWarnedNoData.store(false, std::memory_order_relaxed);

    auto doWrite = [&]()
    {
        // cur == -1 on the very first write yields next = (-1 + 1) % N == 0.
        // (cur + 1) is computed as a plain int before the modulo, so no negative
        // modulo ever occurs; the first write always lands in slot 0.
        int cur  = mWriteCursor.load(std::memory_order_relaxed);
        int next = (cur + 1) % static_cast<int>(kRingCapacity);
        mSlots[next].data        = data;
        mSlots[next].timestampUs = timestamp.AsLongLongInMicroseconds();
        mSlots[next].hasData     = true;
        mWriteCursor.store(next, std::memory_order_release);
    };

    if (mMultiWriter)
    {
        std::lock_guard<std::mutex> lock(mWriteMutex);
        doWrite();
    }
    else
    {
        doWrite();
    }
}

template<typename T>
inline const T* FrameStreamStore<T>::FetchLatest() const
{
    int cur = mWriteCursor.load(std::memory_order_acquire);
    if (cur >= 0 && mSlots[cur].hasData)
    {
        mWarnedNoData.store(false, std::memory_order_relaxed);
        return &(*mSlots[cur].data);
    }
    if (!mWarnedNoData.exchange(true, std::memory_order_relaxed))
        FrameStream_WarnNoData(mId);
    return nullptr;
}

template<typename T>
inline const T* FrameStreamStore<T>::FetchClosestTo(const Dia::Core::TimeAbsolute& time) const
{
    int cur = mWriteCursor.load(std::memory_order_acquire);
    if (cur < 0)
    {
        if (!mWarnedNoData.exchange(true, std::memory_order_relaxed))
            FrameStream_WarnNoData(mId);
        return nullptr;
    }

    const long long targetUs = time.AsLongLongInMicroseconds();
    int bestIdx = -1;
    long long bestAbsDelta = 0;

    int idx = cur;
    for (unsigned int i = 0; i < kRingCapacity; ++i)
    {
        if (!mSlots[idx].hasData)
            break;   // walking backward from cur, an unwritten slot means we've reached
                     // the startup boundary (ring hasn't wrapped yet) — nothing older.
                     // Safe because Write() never resets hasData back to false; once the
                     // ring has fully wrapped, every slot stays hasData == true forever.

        const long long delta    = mSlots[idx].timestampUs - targetUs;
        const long long absDelta = (delta < 0) ? -delta : delta;
        if (bestIdx < 0 || absDelta < bestAbsDelta)
        {
            bestAbsDelta = absDelta;
            bestIdx = idx;
        }

        idx = (idx == 0) ? static_cast<int>(kRingCapacity) - 1 : idx - 1;   // step backward, wrap at 0
    }

    if (bestIdx < 0)
    {
        if (!mWarnedNoData.exchange(true, std::memory_order_relaxed))
            FrameStream_WarnNoData(mId);
        return nullptr;
    }

    mWarnedNoData.store(false, std::memory_order_relaxed);
    return &(*mSlots[bestIdx].data);
}

template<typename T>
inline const Dia::Core::StringCRC& FrameStreamStore<T>::GetId() const
{
    return mId;
}

}} // namespace Dia::ApplicationFlow
