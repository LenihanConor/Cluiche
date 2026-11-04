#pragma once

// CellHistoryRecorder: samples the live cell layers of one grid into the three history tiers (AggregateStore
// aggregates, CellRing per-cell values for tracked layers, EventLog events) and exposes the read API.
// Same-PU contract: construct, Sample, Emit and read all on the registry's PU (SimPU); no locking. No singletons.
//
// Recorded layers: every float and category layer registered on the recorded grid (HistoryConfig::mGrid, else the
// grid of the first registered layer), except layers whose schema sets `history.aggregate: false`. The first
// min(HistoryConfig::mMaxAggregateLayers, 64) in registration order are aggregated; the rest log ONE Warning (for
// the whole recorder) and are skipped (SkippedLayerCount()).
// Layout resolution is lazy and repeated on every Sample, but AggregateStore freezes its row layout at the first
// sample it records, so a layer first seen after that point is skipped with one Warning per layer. In practice games
// register layers before the first Sample.
// Values are ALWAYS read through the layer's own `read` (live values), never through the registry's
// IViewValueSource (the scrubbed view).
// Groups: the implicit `world` group plus `<layerId>=<enumId>` for each enum value of every category layer whose
// schema sets `history.group` (<= 127 non-world groups; extra groups log one Warning and are skipped). Membership is
// re-evaluated from the grouping layer whenever its version() changes. Group sizes are stored as uint16 (clamped).
// A category layer without schema enum values aggregates over 256 ids. A null schema means: no groups, no schema
// flags, and float layers have no range, so per-cell tracking of float layers is impossible (Error).
// Version gate: a layer whose version() is unchanged since the previous sample is not read again; its aggregate row
// is copied from the previous sample (unless a grouping layer changed, which recomputes every row from cached
// values). Tracked/grouping layers keep a private copy of their last read values for this. The gate memory is reset
// by TruncateAfter.
// A Sample with no recorded layers logs ONE Warning (per recorder) and records only the header (and commits events).

#include <DiaCellHistory/AggregateStore.h>
#include <DiaCellHistory/CellRing.h>
#include <DiaCellHistory/EventLog.h>
#include <DiaCellHistory/HistoryConfig.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String32.h>
#include <cstdint>

namespace Dia::CellLayers { class CellLayerRegistry; struct CellLayerDesc; }
namespace Dia::ValueSchema { class ValueSchemaService; }
namespace Dia::Graphics { struct CellGridDesc; }

namespace Dia::CellHistory
{
	struct SampleInfo
	{
		uint64_t		tick = 0;
		const char*		label = nullptr;	// copied (truncated to 31 chars)
		uint8_t			band = 0;			// indexes the configured band names
	};

	struct SampleResult
	{
		uint32_t	mSample = 0;			// the sample index (monotonic, starts at 0)
		uint32_t	mLayersRecorded = 0;	// aggregated layers written for this sample
	};

	struct SampleHeader
	{
		uint64_t						mTick = 0;
		Dia::Core::Containers::String32	mLabel;
		uint8_t							mBand = 0;
	};

	// Byte budgets. Normally derived from HistoryConfig (MB x 1,048,576); the override exists for tests that need a
	// budget below 1 MB to force eviction (see Testing/BudgetSizing.h).
	struct RecorderBudgets
	{
		uint64_t	mCellBytes = 0;
		uint64_t	mAggregateBytes = 0;
		uint64_t	mEventBytes = 0;
	};

	// Committed events of [from, to]. Valid while the recorder lives and until the next Emit/Sample/TruncateAfter.
	struct EventRange
	{
		const EventLog*	mLog = nullptr;
		uint32_t		mFrom = 0;
		uint32_t		mTo = 0;
		bool		Covered() const { return mLog != nullptr && mLog->CoversSamples(mFrom, mTo); }	// false if any part was evicted
		uint32_t	ForEach(EventVisitor visitor, void* context) const { return mLog ? mLog->ForEachEvent(mFrom, mTo, visitor, context) : 0u; }
		uint32_t	Count() const;
	};

	class CellHistoryRecorder
	{
	public:
		static constexpr uint32_t kMaxRecordedLayers = 128;		// CellLayerRegistry::kMaxLayers
		static constexpr uint32_t kDefaultHeaderWindow = 4096;	// header retention while no aggregate window exists

		// runIdSeed: caller-supplied start counter (e.g. wall-clock ticks at module start). RunId() is a fixed
		// SplitMix64 mix of it, so the same seed always gives the same id. schema may be null. Not owned.
		CellHistoryRecorder(Dia::CellLayers::CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService* schema,
			const HistoryConfig& config, uint64_t runIdSeed = 0, const RecorderBudgets* budgetOverride = nullptr);
		~CellHistoryRecorder();
		CellHistoryRecorder(const CellHistoryRecorder&) = delete;
		CellHistoryRecorder& operator=(const CellHistoryRecorder&) = delete;

		SampleResult Sample(const SampleInfo& info);
		uint64_t RunId() const { return mRunId; }

		// Events are assigned to the next sample (== SampleCount() at emit time).
		// Debug builds (DIA_DEBUG) validate the type and payload against the schema service's `kind: event` entry:
		// ONE Warning per event type in total (a type that warned for one reason never warns for another), and the event
		// is still recorded. Checks: type known; payload parses as a JSON object; no undeclared field; declared fields
		// have the schema type (float/int numeric, bool, string/enum/ref string, ref_list array, map object); an `enum`
		// field's value is in the event's vocabulary (when the vocabulary is non-empty). Missing fields are fine
		// (payloads are partial); a null/empty payload is fine; a payload on a type with no payload fields is a
		// mismatch. A null schema service disables validation. A payload over 1 KB is rejected by the EventLog in all
		// builds. Distinct warned types are capped at kMaxWarnedTypes; beyond that events are still recorded and
		// counted in EventValidationWarningsSuppressed(). Both counters read 0 in release builds.
		EmitResult Emit(const HistoryEvent& e);
		static constexpr uint32_t kMaxWarnedTypes = 64;
		uint32_t EventTypeWarningCount() const { return mWarnedTypeCount; }				// distinct types warned
		uint32_t EventValidationWarningsSuppressed() const { return mValidationSuppressed; }	// events that failed validation but could not warn (set full)

		// Minimal tracking hook used for config / schema `history.track` layers (those are tracked automatically as
		// layers are resolved). Requires the layout to exist and the layer to be aggregated. Float layers need a schema
		// min/max. Track() below is the validated run-time entry point.
		bool TrackLayer(Dia::Core::StringCRC layer);

		// Run-time tracking (call between samples, on the registry's PU). Validates EVERY layer first and applies
		// all-or-nothing: a layer that does not exist, is on another grid, is not a recorded (aggregated) layer, or is a
		// float layer without a schema min<max range logs an Error naming it and changes nothing. Already tracked
		// layers are an idempotent success. An empty list is rejected. On success mWindowLength is the new per-cell
		// window; the new layers have data from the next Sample on (earlier samples read false from CellValues).
		// Untrack() frees the rings; a layer that is unknown or not tracked rejects the whole call.
		// If the ring cannot take a layer (window of 0), the layers added by this call are rolled back.
		struct TrackResult
		{
			static constexpr uint32_t kErrorChars = 192;
			bool		mOk = false;
			uint32_t	mWindowLength = 0;
			uint32_t	mChanged = 0;				// layers newly tracked / untracked by this call
			char		mError[kErrorChars] = {};	// first problem when !mOk
		};
		bool Track(const Dia::Core::StringCRC* layers, uint32_t n, TrackResult& out);
		bool Untrack(const Dia::Core::StringCRC* layers, uint32_t n, TrackResult& out);
		bool IsTracked(Dia::Core::StringCRC layer) const { return mRing != nullptr && mRing->IsTracked(layer); }

		// Snapshot for cells.history.status. Fixed size; gap lists are capped at the stores' own caps (64).
		struct StatusGap { uint32_t mFirst = 0; uint32_t mLast = 0; };
		struct HistoryStatus
		{
			static constexpr uint32_t kMaxGaps = 64;
			static constexpr uint32_t kMaxTracked = 64;
			uint32_t	mSampleCount = 0;
			uint64_t	mRunId = 0;
			uint32_t	mRecordedLayers = 0;
			uint32_t	mSkippedLayers = 0;
			uint64_t	mVersionGatedSkips = 0;
			uint32_t	mWindowLength = 0;			// per-cell window (0 while nothing is tracked)
			uint32_t	mAggregateWindow = 0;
			// Retained ranges ([first, latest]; retained == 0 means empty).
			uint32_t	mCellFirst = 0, mCellLatest = 0, mCellRetained = 0;
			uint32_t	mAggregateFirst = 0, mAggregateLatest = 0, mAggregateRetained = 0;
			uint32_t	mEventFirst = 0, mEventLatest = 0, mEventCount = 0;
			uint64_t	mCellBytes = 0, mAggregateBytes = 0, mEventBytes = 0;
			uint64_t	mCellBudget = 0, mAggregateBudget = 0, mEventBudget = 0;
			uint32_t	mTrackedCount = 0;
			Dia::Core::StringCRC	mTracked[kMaxTracked];
			uint32_t	mCellGapCount = 0, mAggregateGapCount = 0, mEventGapCount = 0;
			StatusGap	mCellGaps[kMaxGaps];
			StatusGap	mAggregateGaps[kMaxGaps];
			StatusGap	mEventGaps[kMaxGaps];
		};
		void BuildStatus(HistoryStatus& out) const;

		// Drops every sample with index > sample from all tiers and the headers; the next Sample gets index sample+1.
		void TruncateAfter(uint32_t sample);

		uint32_t SampleCount() const { return mSampleCount; }
		uint32_t FirstRetained() const { return mRing ? mRing->FirstRetained() : 0u; }	// per-cell window start
		uint32_t AggregateFirstRetained() const { return mAggregates.FirstRetained(); }
		bool HasHeader(uint32_t sample) const;
		const SampleHeader& Header(uint32_t sample) const;	// a static empty header for an unknown sample
		bool Aggregate(uint32_t sample, Dia::Core::StringCRC layer, Dia::Core::StringCRC group, AggregateStats& out) const;
		bool CategoryCounts(uint32_t sample, Dia::Core::StringCRC layer, Dia::Core::StringCRC group, uint16_t* out, uint32_t n) const;
		bool CellValues(uint32_t sample, Dia::Core::StringCRC layer, float* out, uint32_t n) const;
		EventRange Events(uint32_t fromSample, uint32_t toSample) const;

		uint32_t RecordedLayerCount() const { return mAggregatedCount; }	// aggregated layers
		// Id of the i-th aggregated (recorded) layer in registration order; a default StringCRC when i >= RecordedLayerCount().
		Dia::Core::StringCRC RecordedLayerId(uint32_t i) const;
		uint32_t SkippedLayerCount() const { return mSkippedLayers; }
		uint64_t VersionGatedSkips() const { return mVersionGatedSkips; }	// layer-samples whose read was avoided
		uint32_t GroupCount() const { return mAggregates.GroupCount(); }
		uint32_t CellCount() const { return mCellCount; }

		const AggregateStore& Aggregates() const { return mAggregates; }
		const EventLog& EventsLog() const { return mEvents; }
		const CellRing* Ring() const { return mRing; }

	private:
		struct Recorded
		{
			Dia::Core::StringCRC	mId;
			bool		mAggregated = false;	// has aggregate rows
			bool		mIsGroup = false;		// history.group category layer
			bool		mCategory = false;
			bool		mTracked = false;
			bool		mHasRange = false;
			float		mMin = 0.f, mMax = 1.f;
			bool		mHasVersion = false;
			uint64_t	mVersion = 0;
			bool		mChanged = false;
			bool		mReadThisSample = false;
			bool		mCacheValid = false;
			float*		mCache = nullptr;
		};
		struct GroupDef
		{
			Dia::Core::StringCRC	mId;
			uint32_t	mSourceLayer = 0;	// index into mRecorded
			uint32_t	mValue = 0;			// enum index
			uint16_t	mSize = 0;
		};

		bool TrackRecorded(Recorded& r);
		bool ResolveGrid();
		void ResolveLayers();
		void AddLayer(const Dia::CellLayers::CellLayerDesc& desc);
		int FindRecorded(Dia::Core::StringCRC id) const;
		bool Seen(uint32_t crc) const;
		void MarkSeen(uint32_t crc);
		bool NeedsCache(const Recorded& r) const;
		const float* ReadValues(Recorded& r, const Dia::CellLayers::CellLayerDesc& desc, uint64_t version);
		const float* ValuesFor(Recorded& r, const Dia::CellLayers::CellLayerDesc& desc, uint64_t version);
		void RebuildMasks();
		void PushHeader(uint32_t sample, const SampleInfo& info);
		void ResetGate();
		void ValidateEvent(const HistoryEvent& e);
		void WarnEventOnce(const HistoryEvent& e, const char* reason);

		uint32_t		mWarnedTypes[kMaxWarnedTypes];
		uint32_t		mWarnedTypeCount = 0;
		uint32_t		mValidationSuppressed = 0;

		Dia::CellLayers::CellLayerRegistry&			mRegistry;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		HistoryConfig	mConfig;
		RecorderBudgets	mBudgets;
		uint64_t		mRunId;

		AggregateStore	mAggregates;
		EventLog		mEvents;
		CellRing*		mRing = nullptr;
		const Dia::Graphics::CellGridDesc* mGrid = nullptr;
		uint32_t		mCellCount = 0;
		bool			mGridErrorLogged = false;

		uint32_t		mSampleCount = 0;
		bool			mFrozen = false;
		bool			mNoLayersWarned = false;
		bool			mCapWarned = false;
		bool			mGroupCapWarned = false;
		uint32_t		mSkippedLayers = 0;
		uint64_t		mVersionGatedSkips = 0;

		Recorded		mRecorded[kMaxRecordedLayers];
		uint32_t		mRecordedCount = 0;
		uint32_t		mAggregatedCount = 0;
		uint32_t		mSeen[kMaxRecordedLayers];
		uint32_t		mSeenCount = 0;
		bool			mGroupsDirty = true;
		bool			mHasGroups = false;

		GroupDef		mGroupDefs[AggregateStore::kMaxGroups];
		uint32_t		mGroupDefCount = 0;		// non-world groups
		uint8_t*		mMasks = nullptr;		// kMaxGroups * mCellCount, allocated with the grid
		float*			mScratch = nullptr;		// mCellCount floats for layers without a cache

		SampleHeader*	mHeaders = nullptr;
		uint32_t		mHeaderCap = 0;
		uint32_t		mHeaderHead = 0;
		uint32_t		mHeaderCount = 0;
		uint32_t		mHeaderFirst = 0;		// sample index of the oldest retained header
	};
}
