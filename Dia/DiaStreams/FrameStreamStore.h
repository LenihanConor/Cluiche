#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <chrono>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaStreams/IStreamStore.h>
#include <DiaStreams/FrameStreamDiagnostics.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/MetricInfo.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>

namespace Dia { namespace ApplicationFlow {

// StreamLink: trace link captured at write time, exposed to readers.
struct StreamLink
{
    uint64_t traceId;
    uint64_t spanId;
    uint64_t writeSteadyNs;
    bool valid;
};

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
    const T* FetchLatestLinked(StreamLink& outLink) const;
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
        // Trace-link fields: writer's trace and span id at Write() time. Zero when
        // the tracer is not started or no span is open. Always compiled; tracer-gated.
        uint64_t traceId       = 0;
        uint64_t spanId        = 0;
        uint64_t writeSteadyNs = 0;
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

    // Edge statistics (Task 5): per-stream metrics for write/read cost and data age.
    // Mutable: updated from const Fetch methods.
    mutable std::atomic<uint64_t> mEdgeWrites{0};
    mutable std::atomic<uint64_t> mEdgeBytes{0};
    mutable std::atomic<uint64_t> mEdgeLastWriteSteadyNs{0};
    mutable std::atomic<uint64_t> mEdgeWriteNsTotal{0};
    mutable std::atomic<uint64_t> mEdgeReads{0};
    mutable std::atomic<uint64_t> mEdgeReadNsTotal{0};
    Dia::Observation::Metric::Counter* mMetricWrites    = nullptr;
    Dia::Observation::Metric::Counter* mMetricBytes     = nullptr;
    Dia::Observation::Metric::Gauge*   mMetricDataAgeNs = nullptr;
    Dia::Observation::Metric::Gauge*   mMetricWriteCostNs = nullptr;
    Dia::Observation::Metric::Gauge*   mMetricReadCostNs  = nullptr;
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
    // Register edge statistics metrics (Task 5)
    using namespace Dia::Observation::Metric;
    auto& registry = MetricRegistry::Instance();

    char nameBuf[256];
    std::snprintf(nameBuf, sizeof(nameBuf), "stream.frame.%s.writes", id.AsChar());
    mMetricWrites = registry.RegisterCounter(Dia::Core::StringCRC(nameBuf),
        MetricInfo{"Writes", "Number of Write() calls", "count"});

    std::snprintf(nameBuf, sizeof(nameBuf), "stream.frame.%s.bytes", id.AsChar());
    mMetricBytes = registry.RegisterCounter(Dia::Core::StringCRC(nameBuf),
        MetricInfo{"Bytes", "Total bytes written (payload size)", "bytes"});

    std::snprintf(nameBuf, sizeof(nameBuf), "stream.frame.%s.data_age_ns", id.AsChar());
    mMetricDataAgeNs = registry.RegisterGauge(Dia::Core::StringCRC(nameBuf),
        MetricInfo{"Data Age", "Steady time between write and read", "nanoseconds"});

    std::snprintf(nameBuf, sizeof(nameBuf), "stream.frame.%s.write_cost_ns", id.AsChar());
    mMetricWriteCostNs = registry.RegisterGauge(Dia::Core::StringCRC(nameBuf),
        MetricInfo{"Write Cost", "Average steady time per Write() call", "nanoseconds"});

    std::snprintf(nameBuf, sizeof(nameBuf), "stream.frame.%s.read_cost_ns", id.AsChar());
    mMetricReadCostNs = registry.RegisterGauge(Dia::Core::StringCRC(nameBuf),
        MetricInfo{"Read Cost", "Average steady time per FetchLatestLinked() call", "nanoseconds"});
}

template<typename T>
inline void FrameStreamStore<T>::Write(const T& data, const Dia::Core::TimeAbsolute& timestamp)
{
    mWarnedNoData.store(false, std::memory_order_relaxed);

    // Capture trace link if tracer is active (one flag test when off; zero fields otherwise)
    Dia::Observation::Trace::TraceLink link = Dia::Observation::Trace::Tracer::GetCurrentLink();
    uint64_t writeStartNs = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    auto doWrite = [&]()
    {
        // cur == -1 on the very first write yields next = (-1 + 1) % N == 0.
        // (cur + 1) is computed as a plain int before the modulo, so no negative
        // modulo ever occurs; the first write always lands in slot 0.
        int cur  = mWriteCursor.load(std::memory_order_relaxed);
        int next = (cur + 1) % static_cast<int>(kRingCapacity);
        mSlots[next].data          = data;
        mSlots[next].timestampUs   = timestamp.AsLongLongInMicroseconds();
        mSlots[next].hasData       = true;
        mSlots[next].traceId       = link.traceId;
        mSlots[next].spanId        = link.spanId;
        mSlots[next].writeSteadyNs = writeStartNs;
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

    // Edge statistics (Task 5): update after write
    uint64_t writeEndNs = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    uint64_t writeCostNs = writeEndNs - writeStartNs;

    mEdgeWrites.fetch_add(1, std::memory_order_relaxed);
    mEdgeBytes.fetch_add(sizeof(T), std::memory_order_relaxed);
    mEdgeLastWriteSteadyNs.store(writeStartNs, std::memory_order_relaxed);
    mEdgeWriteNsTotal.fetch_add(writeCostNs, std::memory_order_relaxed);

    // Update gauges (average cost)
    if (mMetricWrites)
        mMetricWrites->Inc();
    if (mMetricBytes)
        mMetricBytes->Inc(sizeof(T));
    if (mMetricWriteCostNs)
    {
        uint64_t writes = mEdgeWrites.load(std::memory_order_relaxed);
        uint64_t totalNs = mEdgeWriteNsTotal.load(std::memory_order_relaxed);
        if (writes > 0)
            mMetricWriteCostNs->Set(static_cast<double>(totalNs) / static_cast<double>(writes));
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
inline const T* FrameStreamStore<T>::FetchLatestLinked(StreamLink& outLink) const
{
    uint64_t readStartNs = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    int cur = mWriteCursor.load(std::memory_order_acquire);
    if (cur >= 0 && mSlots[cur].hasData)
    {
        mWarnedNoData.store(false, std::memory_order_relaxed);
        outLink.traceId       = mSlots[cur].traceId;
        outLink.spanId        = mSlots[cur].spanId;
        outLink.writeSteadyNs = mSlots[cur].writeSteadyNs;
        outLink.valid         = (mSlots[cur].traceId != 0 && mSlots[cur].spanId != 0);

        // Edge statistics (Task 5): update after read
        uint64_t readEndNs = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        uint64_t readCostNs = readEndNs - readStartNs;
        uint64_t dataAgeNs = readStartNs - mSlots[cur].writeSteadyNs;

        mEdgeReads.fetch_add(1, std::memory_order_relaxed);
        mEdgeReadNsTotal.fetch_add(readCostNs, std::memory_order_relaxed);

        // Update gauges
        if (mMetricDataAgeNs)
            mMetricDataAgeNs->Set(static_cast<double>(dataAgeNs));
        if (mMetricReadCostNs)
        {
            uint64_t reads = mEdgeReads.load(std::memory_order_relaxed);
            uint64_t totalNs = mEdgeReadNsTotal.load(std::memory_order_relaxed);
            if (reads > 0)
                mMetricReadCostNs->Set(static_cast<double>(totalNs) / static_cast<double>(reads));
        }

        return &(*mSlots[cur].data);
    }
    if (!mWarnedNoData.exchange(true, std::memory_order_relaxed))
        FrameStream_WarnNoData(mId);
    outLink.valid = false;
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
