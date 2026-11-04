#pragma once

// TimelineModel: pure data builder for the history timeline card (Feature 2). Reads the recorder's read API and the
// view state; no UI, no drawing, no singletons. Same-PU contract as the recorder (build on the registry's PU).
//
// The timeline covers the whole run [0, latest] (aggregates cover every sample). Build() produces:
//   - range / position / live marker;
//   - buckets: bucketCount buckets partitioning [0, latest] exactly (each sample in exactly one bucket). Bucket b of n
//     over `count` samples covers [ceil(b*count/n), ceil((b+1)*count/n) - 1]. bucketCount is clamped to [1, 4096] and
//     to the sample count (never an empty bucket). Callers pass kDefaultBucketCount (256) or the card's pixel width.
//     A bucket holding >= 1 event always carries eventCount >= 1 and a colour (events are never hidden at coarse zoom);
//     it lists up to kMaxTypesPerBucket per-type counts (highest first, ties by first appearance) and takes the colour
//     of the dominant type. Event colour: schema `ui.colour` ("#rrggbb") of the event's `kind: event` entry, else
//     kDefaultEventColour (0xRRGGBBAA);
//   - band ticks: every sample whose band is > 0 (band 0 is the finest band and marks every sample), with the band name
//     from the config (else "band<N>"), capped at kMaxBandTicks (BandTicksTruncated());
//   - gaps (aggregate / per-cell / event ranges evicted), from CellHistoryRecorder::BuildStatus;
//   - the pre-window dim range [0, FirstRetained) when a per-cell window exists and has moved past sample 0;
//   - a sparkline of the base layer's world mean per bucket (<= kMaxSparkPoints points). Missing / NaN aggregates make a
//     point invalid (a gap) rather than zero; a layer with no aggregate data at all gives an empty sparkline.
// Rebuild only when NeedsRebuild() (sample recorded, view changed, base layer changed), not every frame.
// Known limit: the stamp is the sample COUNT, so TruncateAfter followed by re-sampling back to the same count (with the
// view untouched) is not detected; the recorder exposes no truncate generation. Callers that truncate (restore) must force a rebuild.

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String32.h>
#include <json/json.h>
#include <cstdint>

namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellHistory
{
	struct TimelineRange { uint32_t mFirstSample = 0; uint32_t mLatestSample = 0; uint32_t mCount = 0; };
	struct TimelinePosition { ViewMode mMode = ViewMode::Live; uint32_t mSample = 0; };

	struct TimelineTypeCount
	{
		Dia::Core::CRC	mType;
		uint32_t		mCount = 0;
		uint32_t		mColour = 0;	// 0xRRGGBBAA
	};

	struct TimelineBucket
	{
		static constexpr uint32_t kMaxTypesPerBucket = 4;
		uint32_t			mFirstSample = 0;
		uint32_t			mLastSample = 0;
		uint32_t			mEventCount = 0;	// all events in the bucket (>= sum of the listed type counts)
		uint32_t			mColour = 0;		// dominant type colour; 0 when no events
		uint32_t			mTypeCount = 0;		// valid entries of mTypes
		TimelineTypeCount	mTypes[kMaxTypesPerBucket];
	};

	struct TimelineBandTick { uint32_t mSample = 0; uint8_t mBand = 0; Dia::Core::Containers::String32 mName; };
	struct TimelineSpan { bool mPresent = false; uint32_t mFirst = 0; uint32_t mLast = 0; };
	struct TimelineSparkPoint { uint32_t mFirstSample = 0; uint32_t mLastSample = 0; float mValue = 0.f; bool mValid = false; };


	// Minimal growable array (trivially-relocatable element types only: plain data and the fixed-size string types).
	template <class T>
	class TimelineArray
	{
	public:
		TimelineArray() = default;
		TimelineArray(const TimelineArray& o) { Copy(o); }
		TimelineArray& operator=(const TimelineArray& o) { if (this != &o) { Copy(o); } return *this; }
		~TimelineArray() { delete[] mData; }
		uint32_t Size() const { return mSize; }
		void RemoveAll() { mSize = 0; }
		void Reserve(uint32_t cap)
		{
			if (cap <= mCap) { return; }
			T* d = new T[cap];
			for (uint32_t i = 0; i < mSize; ++i) { d[i] = mData[i]; }
			delete[] mData;
			mData = d;
			mCap = cap;
		}
		void Add(const T& v) { if (mSize == mCap) { Reserve(mCap == 0 ? 16u : mCap * 2u); } mData[mSize++] = v; }
		T& operator[](uint32_t i) { return mData[i]; }
		const T& operator[](uint32_t i) const { return mData[i]; }
	private:
		void Copy(const TimelineArray& o)
		{
			mSize = 0;
			Reserve(o.mSize);
			for (uint32_t i = 0; i < o.mSize; ++i) { mData[i] = o.mData[i]; }
			mSize = o.mSize;
		}
		T*			mData = nullptr;
		uint32_t	mSize = 0;
		uint32_t	mCap = 0;
	};

	class TimelineModel
	{
	public:
		static constexpr uint32_t kDefaultBucketCount = 256;
		static constexpr uint32_t kMaxBucketCount = 4096;
		static constexpr uint32_t kMaxSparkPoints = 256;
		static constexpr uint32_t kMaxBandTicks = 4096;
		static constexpr uint32_t kDefaultEventColour = 0x9E9E9EFFu;

		// Reads the recorder, the view and (optional) schema / config. baseLayer: the layer for the sparkline.
		void Build(const CellHistoryRecorder& recorder, const HistoryViewState& view, Dia::Core::StringCRC baseLayer,
			const Dia::ValueSchema::ValueSchemaService* schema, const HistoryConfig* config, uint32_t bucketCount = kDefaultBucketCount);

		// True when never built, or SampleCount, the view epoch or the base layer changed since the last Build.
		bool NeedsRebuild(const CellHistoryRecorder& recorder, const HistoryViewState& view, Dia::Core::StringCRC baseLayer) const;

		double LastBuildMs() const { return mLastBuildMs; }
		uint64_t BuildCount() const { return mBuildCount; }

		const TimelineRange&	Range() const { return mRange; }
		const TimelinePosition&	Position() const { return mPosition; }
		bool					HasLiveMarker() const { return mRange.mCount > 0; }
		uint32_t				LiveMarker() const { return mRange.mLatestSample; }

		uint32_t				BucketCount() const { return mBuckets.Size(); }
		const TimelineBucket&	Bucket(uint32_t i) const { return mBuckets[i]; }
		uint32_t				BandTickCount() const { return mBandTicks.Size(); }
		const TimelineBandTick&	BandTick(uint32_t i) const { return mBandTicks[i]; }
		bool					BandTicksTruncated() const { return mBandTicksTruncated; }

		uint32_t				AggregateGapCount() const { return mAggregateGapCount; }
		uint32_t				CellGapCount() const { return mCellGapCount; }
		uint32_t				EventGapCount() const { return mEventGapCount; }
		CellHistoryRecorder::StatusGap AggregateGap(uint32_t i) const { return i < mAggregateGapCount ? mAggregateGaps[i] : CellHistoryRecorder::StatusGap(); }
		CellHistoryRecorder::StatusGap CellGap(uint32_t i) const { return i < mCellGapCount ? mCellGaps[i] : CellHistoryRecorder::StatusGap(); }
		CellHistoryRecorder::StatusGap EventGap(uint32_t i) const { return i < mEventGapCount ? mEventGaps[i] : CellHistoryRecorder::StatusGap(); }

		const TimelineSpan&		PreWindowDim() const { return mPreWindow; }

		uint32_t					SparklineCount() const { return mSpark.Size(); }
		const TimelineSparkPoint&	SparklinePoint(uint32_t i) const { return mSpark[i]; }

		// Card JSON (range, position, live, buckets, bandTicks, gaps, preWindowDim, sparkline).
		Json::Value ToJson() const;

	private:
		TimelineRange	mRange;
		TimelinePosition mPosition;
		TimelineArray<TimelineBucket>		mBuckets;
		TimelineArray<TimelineBandTick>	mBandTicks;
		TimelineArray<TimelineSparkPoint>	mSpark;
		bool			mBandTicksTruncated = false;
		CellHistoryRecorder::StatusGap mAggregateGaps[CellHistoryRecorder::HistoryStatus::kMaxGaps];
		CellHistoryRecorder::StatusGap mCellGaps[CellHistoryRecorder::HistoryStatus::kMaxGaps];
		CellHistoryRecorder::StatusGap mEventGaps[CellHistoryRecorder::HistoryStatus::kMaxGaps];
		uint32_t		mAggregateGapCount = 0, mCellGapCount = 0, mEventGapCount = 0;
		TimelineSpan	mPreWindow;

		bool			mBuilt = false;
		uint32_t		mStampSamples = 0;
		uint64_t		mStampEpoch = 0;
		Dia::Core::StringCRC mStampLayer;
		double			mLastBuildMs = 0.0;
		uint64_t		mBuildCount = 0;
	};
}
