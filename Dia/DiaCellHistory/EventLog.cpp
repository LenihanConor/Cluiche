#include <DiaCellHistory/EventLog.h>
#include <DiaCellHistory/Utf8.h>
#include <DiaObservation/Log/DiaLog.h>
#include <algorithm>
#include <cstring>

namespace Dia::CellHistory
{
	namespace
	{
		uint32_t Align4(uint32_t n) { return (n + 3u) & ~3u; }
	}

	EventLog::EventLog(uint64_t budgetBytes) : mBudget(budgetBytes)
	{
		static_assert(sizeof(Header) == kHeaderBytes, "EventLog header layout");
	}

	EventLog::~EventLog() { delete[] mData; }

	double EventLog::UsedFraction() const
	{
		return mBudget == 0 ? 0.0 : double(BytesUsed()) / double(mBudget);
	}

	EventGap EventLog::Gap(uint32_t i) const { return i < mGapCount ? mGaps[i] : EventGap(); }

	static size_t BoundedLen(const char* s, size_t cap)
	{
		if (s == nullptr) { return 0; }
		size_t n = 0;
		while (n < cap && s[n] != '\0') { ++n; }
		return n;
	}

	uint32_t EventLog::RecordBytes(const HistoryEvent& e)
	{
		const size_t p = BoundedLen(e.payloadJson, kMaxPayloadBytes + 1);
		const size_t t = Utf8SafeLength(e.text, kMaxTextChars);
		return Align4(uint32_t(kHeaderBytes + p + t));
	}

	void EventLog::WriteAt(uint64_t off, const void* src, size_t n)
	{
		const uint8_t* s = static_cast<const uint8_t*>(src);
		const size_t phys = size_t(off % mCap);
		const size_t first = std::min(n, size_t(mCap) - phys);
		std::memcpy(mData + phys, s, first);
		if (first < n) { std::memcpy(mData, s + first, n - first); }
	}

	void EventLog::ReadAt(uint64_t off, void* dst, size_t n) const
	{
		uint8_t* d = static_cast<uint8_t*>(dst);
		const size_t phys = size_t(off % mCap);
		const size_t first = std::min(n, size_t(mCap) - phys);
		std::memcpy(d, mData + phys, first);
		if (first < n) { std::memcpy(d + first, mData, n - first); }
	}

	void EventLog::Grow(uint64_t needed)
	{
		if (needed <= mCap) { return; }
		uint64_t newCap = std::max<uint64_t>({ needed, mCap * 2, 256 });
		if (newCap > mBudget) { newCap = std::max(mBudget, needed); }
		uint8_t* fresh = new uint8_t[size_t(newCap)];
		const uint64_t used = mTail - mHead;
		if (used > 0) { ReadAt(mHead, fresh, size_t(used)); }
		delete[] mData;
		mData = fresh;
		mCap = newCap;
		mCommittedEnd = mCommittedEnd - mHead;
		mTail = used;
		mHead = 0;
	}

	void EventLog::RecordDrop(uint32_t sample)
	{
		if (mGapOpen && mGapCount > 0)
		{
			mGaps[mGapCount - 1].mLastDropped = std::max(mGaps[mGapCount - 1].mLastDropped, sample);
		}
		else if (mGapCount > 0 && sample <= mGaps[mGapCount - 1].mLastDropped)
		{
			mGapOpen = true;
		}
		else
		{
			if (mGapCount == kMaxGaps)
			{
				for (uint32_t i = 1; i < kMaxGaps; ++i) { mGaps[i - 1] = mGaps[i]; }
				--mGapCount;
			}
			mGaps[mGapCount++] = EventGap{ sample, sample };
			mGapOpen = true;
		}
		mFloor = std::max(mFloor, sample + 1);
	}

	void EventLog::DropOldest()
	{
		Header h;
		ReadAt(mHead, &h, sizeof(h));
		mHead += Align4(uint32_t(kHeaderBytes) + h.mPayloadLen + h.mTextLen);
		--mCommittedCount;
		++mDroppedCount;
		RecordDrop(h.mSample);
	}

	EmitResult EventLog::Emit(const HistoryEvent& e)
	{
		const size_t payloadLen = BoundedLen(e.payloadJson, kMaxPayloadBytes + 1);
		if (payloadLen > kMaxPayloadBytes)
		{
			DIA_LOG_ERROR("HIS", "event '%s' payload exceeds %u bytes; rejected", e.type.AsChar(), kMaxPayloadBytes);
			return EmitResult::PayloadTooLarge;
		}
		const size_t textLen = Utf8SafeLength(e.text, kMaxTextChars);
		const uint32_t rec = Align4(uint32_t(kHeaderBytes + payloadLen + textLen));
		if (rec > mBudget)
		{
			DIA_LOG_ERROR("HIS", "event '%s' (%u bytes) cannot fit the event budget (%llu bytes)", e.type.AsChar(), rec, (unsigned long long)mBudget);
			return EmitResult::TooLargeForBudget;
		}
		if ((mTail - mCommittedEnd) + rec > mBudget)
		{
			DIA_LOG_ERROR("HIS", "event '%s' rejected: pending events fill the event budget", e.type.AsChar());
			return EmitResult::PendingFull;
		}

		bool evicted = false;
		while ((mTail - mHead) + rec > mBudget)
		{
			DropOldest();
			evicted = true;
		}
		if (evicted)
		{
			if (!mInEpisode)
			{
				mInEpisode = true;
				++mEpisodeCount;
				DIA_LOG_WARNING("HIS", "event budget exceeded: dropping oldest events (episode %u)", mEpisodeCount);
			}
		}
		else
		{
			mInEpisode = false;
			mGapOpen = false;
		}

		Grow((mTail - mHead) + rec);

		Header h;
		h.mType = e.type.Value();
		h.mLayer = e.group.mLayer.Value();
		h.mValue = e.group.mValue;
		h.mCell = e.cell;
		h.mSample = 0;
		h.mSequence = mNextSequence++;
		h.mPayloadLen = uint16_t(payloadLen);
		h.mTextLen = uint8_t(textLen);
		h.mPad = 0;
		uint64_t off = mTail;
		WriteAt(off, &h, sizeof(h));
		off += sizeof(h);
		if (payloadLen) { WriteAt(off, e.payloadJson, payloadLen); off += payloadLen; }
		if (textLen) { WriteAt(off, e.text, textLen); }
		mTail += rec;
		++mPendingCount;
		return EmitResult::Ok;
	}

	bool EventLog::CommitSample(uint32_t sampleIndex)
	{
		if (mHasCommitted && sampleIndex <= mLatestSample)
		{
			DIA_LOG_ERROR("HIS", "event log: commit sample %u is not after latest %u", sampleIndex, mLatestSample);
			return false;
		}
		uint64_t off = mCommittedEnd;
		while (off < mTail)
		{
			Header h;
			ReadAt(off, &h, sizeof(h));
			h.mSample = sampleIndex;
			WriteAt(off, &h, sizeof(h));
			off += Align4(uint32_t(kHeaderBytes) + h.mPayloadLen + h.mTextLen);
		}
		mCommittedEnd = mTail;
		mCommittedCount += mPendingCount;
		mPendingCount = 0;
		mLatestSample = sampleIndex;
		mHasCommitted = true;
		return true;
	}

	void EventLog::TruncateAfter(uint32_t sample)
	{
		uint64_t off = mHead;
		uint32_t kept = 0;
		uint32_t latest = 0;
		while (off < mCommittedEnd)
		{
			Header h;
			ReadAt(off, &h, sizeof(h));
			if (h.mSample > sample) { break; }
			latest = h.mSample;
			++kept;
			off += Align4(uint32_t(kHeaderBytes) + h.mPayloadLen + h.mTextLen);
		}
		mTail = off;
		mCommittedEnd = off;
		mCommittedCount = kept;
		mPendingCount = 0;
		mHasCommitted = kept > 0;
		mLatestSample = latest;
	}

	void EventLog::Clear()
	{
		mHead = mTail;
		mCommittedEnd = mTail;
		mCommittedCount = 0;
		mPendingCount = 0;
		mNextSequence = 0;
		mLatestSample = 0;
		mHasCommitted = false;
		mFloor = 0;
		mInEpisode = false;
		mEpisodeCount = 0;
		mDroppedCount = 0;
		mGapCount = 0;
		mGapOpen = false;
	}

	uint32_t EventLog::ForEachEvent(uint32_t fromSample, uint32_t toSample, EventVisitor visitor, void* context) const
	{
		if (visitor == nullptr || fromSample > toSample) { return 0; }
		uint32_t visited = 0;
		uint64_t off = mHead;
		char payload[kMaxPayloadBytes + 1];
		char text[kMaxTextChars + 1];
		while (off < mCommittedEnd)
		{
			Header h;
			ReadAt(off, &h, sizeof(h));
			const uint32_t len = Align4(uint32_t(kHeaderBytes) + h.mPayloadLen + h.mTextLen);
			if (h.mSample > toSample) { break; }
			if (h.mSample >= fromSample)
			{
				if (h.mPayloadLen) { ReadAt(off + kHeaderBytes, payload, h.mPayloadLen); }
				payload[h.mPayloadLen] = '\0';
				if (h.mTextLen) { ReadAt(off + kHeaderBytes + h.mPayloadLen, text, h.mTextLen); }
				text[h.mTextLen] = '\0';
				EventView v;
				v.mType = Dia::Core::CRC(h.mType);
				v.mGroupLayer = Dia::Core::CRC(h.mLayer);
				v.mGroupValue = h.mValue;
				v.mCell = h.mCell;
				v.mSample = h.mSample;
				v.mSequence = h.mSequence;
				v.mPayloadJson = payload;
				v.mText = text;
				visitor(v, context);
				++visited;
			}
			off += len;
		}
		return visited;
	}

	bool EventLog::CoversSamples(uint32_t fromSample, uint32_t toSample) const
	{
		if (fromSample > toSample) { return false; }
		if (fromSample < mFloor) { return false; }
		for (uint32_t i = 0; i < mGapCount; ++i)
		{
			if (fromSample <= mGaps[i].mLastDropped && toSample >= mGaps[i].mFirstDropped) { return false; }
		}
		return true;
	}
}
