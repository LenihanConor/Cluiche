#pragma once

// EventLog: Tier-3 event storage. Pure data structure (no STL in this header). The recorder owns the sample counter
// and drives this class: Emit() while systems step, CommitSample(index) when the recorder takes sample `index`.
//
// Assignment: events emitted since the previous CommitSample are PENDING; CommitSample(i) attaches all of them to
// sample i in emit order. Pending events are never returned by reads. Sample indices passed to CommitSample are
// strictly increasing and may skip.
//
// Limits: payload JSON over kMaxPayloadBytes (1024) is REJECTED (Error log, nothing stored). Text over kMaxTextChars
// (64) is TRUNCATED to 64 chars (not rejected). Payload and text are copied into private storage.
//
// Storage: variable-length records in a private byte ring that grows (doubling) up to the budget. A record costs
// RecordBytes() = 28-byte header + payload + text, rounded up to 4. The budget counts committed AND pending events.
// When an Emit would exceed the budget the oldest COMMITTED events are dropped; pending events are never dropped.
// If a single record can never fit the budget, or pending events alone leave no room, Emit rejects (Error log).
// Eviction: one Warning per episode (an episode ends when an Emit needs no eviction); gaps are merged sample-index
// ranges {first,last} dropped, capped at kMaxGaps (oldest discarded). A partially dropped sample counts as dropped.

#include <DiaCore/CRC/StringCRC.h>
#include <cstddef>
#include <cstdint>

namespace Dia::CellHistory
{
	struct GroupRef
	{
		static constexpr uint32_t kNoValue = 0xFFFFFFFFu;
		Dia::Core::StringCRC mLayer;
		uint32_t mValue = kNoValue;		// enum index within the grouping layer
		bool IsNone() const { return mValue == kNoValue; }
	};

	struct HistoryEvent
	{
		static constexpr uint32_t kNoCell = 0xFFFFFFFFu;
		Dia::Core::StringCRC type;
		GroupRef group;
		uint32_t cell = kNoCell;
		const char* payloadJson = nullptr;	// compact JSON object, <= 1024 bytes; may be null
		const char* text = nullptr;			// short text; truncated to 64 chars; may be null
	};

	enum class EmitResult : uint8_t { Ok, PayloadTooLarge, TooLargeForBudget, PendingFull };

	struct EventView
	{
		Dia::Core::CRC mType;
		Dia::Core::CRC mGroupLayer;
		uint32_t mGroupValue = GroupRef::kNoValue;
		uint32_t mCell = HistoryEvent::kNoCell;
		uint32_t mSample = 0;
		uint32_t mSequence = 0;
		const char* mPayloadJson = "";		// null-terminated, empty if none; valid only during the visit
		const char* mText = "";				// null-terminated, empty if none; valid only during the visit
		bool GroupIsNone() const { return mGroupValue == GroupRef::kNoValue; }
	};

	struct EventGap
	{
		uint32_t mFirstDropped = 0;
		uint32_t mLastDropped = 0;
	};

	typedef void (*EventVisitor)(const EventView& event, void* context);

	class EventLog
	{
	public:
		static constexpr uint32_t kMaxPayloadBytes = 1024;
		static constexpr uint32_t kMaxTextChars = 64;
		static constexpr uint32_t kMaxGaps = 64;

		explicit EventLog(uint64_t budgetBytes);
		~EventLog();
		EventLog(const EventLog&) = delete;
		EventLog& operator=(const EventLog&) = delete;

		static uint32_t RecordBytes(const HistoryEvent& e);	// stored size (text counted after truncation)

		EmitResult Emit(const HistoryEvent& e);
		// Attaches all pending events to sampleIndex. false (Error log) if sampleIndex <= latest committed sample.
		bool CommitSample(uint32_t sampleIndex);
		// Drops committed events with sample > sample and ALL pending events.
		void TruncateAfter(uint32_t sample);

		// Visits committed events with fromSample <= sample <= toSample in emit order. Returns the count visited.
		uint32_t ForEachEvent(uint32_t fromSample, uint32_t toSample, EventVisitor visitor, void* context) const;
		// false if any part of [from,to] touches a gap or lies below FirstRetainedSample().
		bool CoversSamples(uint32_t fromSample, uint32_t toSample) const;

		uint32_t EventCount() const { return mCommittedCount; }	// committed only
		uint32_t PendingCount() const { return mPendingCount; }
		uint32_t FirstRetainedSample() const { return mFloor; }	// 0 until something is dropped
		uint32_t LatestSample() const { return mLatestSample; }	// 0 when none committed
		uint64_t BytesUsed() const { return mTail - mHead; }
		double UsedFraction() const;
		uint64_t Budget() const { return mBudget; }

		uint32_t GapCount() const { return mGapCount; }
		EventGap Gap(uint32_t i) const;	// oldest first; zeroed if out of range
		uint32_t EvictionEpisodeCount() const { return mEpisodeCount; }
		uint32_t DroppedEventCount() const { return mDroppedCount; }

	private:
		struct Header
		{
			uint32_t mType, mLayer, mValue, mCell, mSample, mSequence;
			uint16_t mPayloadLen;
			uint8_t mTextLen;
			uint8_t mPad;
		};
		static constexpr uint32_t kHeaderBytes = 28;

		void Grow(uint64_t needed);
		void WriteAt(uint64_t off, const void* src, size_t n);
		void ReadAt(uint64_t off, void* dst, size_t n) const;
		void DropOldest();
		void RecordDrop(uint32_t sample);

		uint64_t mBudget;
		uint8_t* mData = nullptr;
		uint64_t mCap = 0;
		// Logical offsets: [mHead, mCommittedEnd) committed, [mCommittedEnd, mTail) pending.
		uint64_t mHead = 0, mCommittedEnd = 0, mTail = 0;
		uint32_t mCommittedCount = 0, mPendingCount = 0;
		uint32_t mNextSequence = 0;
		uint32_t mLatestSample = 0;
		bool mHasCommitted = false;
		uint32_t mFloor = 0;
		bool mInEpisode = false;
		uint32_t mEpisodeCount = 0;
		uint32_t mDroppedCount = 0;
		EventGap mGaps[kMaxGaps];
		uint32_t mGapCount = 0;
		bool mGapOpen = false;	// last gap belongs to the current episode
	};
}
