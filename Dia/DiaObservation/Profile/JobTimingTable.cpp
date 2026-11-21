#include "JobTimingTable.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		namespace
		{
			uint32_t ClampU32(uint64_t v) { return v > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(v); }
		}

		JobTimingTable::JobTimingTable()
		{
			Reset();
		}

		void JobTimingTable::Init(uint32_t rawCapacity)
		{
			if (rawCapacity < 1) rawCapacity = 1;
			mRaw.assign(rawCapacity, JobRecordPacked{});
			mAgg.assign(kAggregateCapacity, JobFrameAggregate{});
			Reset();
		}

		void JobTimingTable::Reset()
		{
			std::memset(mOwners, 0, sizeof(mOwners));
			mOwnerCount = 0;
			std::memset(&mPool, 0, sizeof(mPool));
			mRecordsSeen = 0;
			mRawTotal = 0;
			mAggTotal = 0;
			std::memset(mSteps, 0, sizeof(mSteps));
			mStepTotal = 0;
			mPerJob = true;
		}

		JobTimingTable::OwnerSlot* JobTimingTable::FindOrAdd(uint32_t owner)
		{
			for (uint32_t i = 0; i < mOwnerCount; ++i)
				if (mOwners[i].owner == owner)
					return &mOwners[i];
			if (mOwnerCount < kMaxOwners - 1)
			{
				OwnerSlot* s = &mOwners[mOwnerCount++];
				s->owner = owner;
				return s;
			}
			// Table full: everything else lands in the overflow bucket (owner 0, last slot).
			mOwnerCount = kMaxOwners;
			mOwners[kMaxOwners - 1].owner = 0;
			return &mOwners[kMaxOwners - 1];
		}

		void JobTimingTable::Record(const Dia::Threading::JobTimingRecord& rec)
		{
			++mRecordsSeen;
			const uint64_t durNs  = rec.endNs > rec.startNs ? rec.endNs - rec.startNs : 0;
			const uint64_t waitNs = rec.startNs > rec.submitNs ? rec.startNs - rec.submitNs : 0;
			const float    waitMs = static_cast<float>(waitNs) * 1e-6f;

			OwnerSlot* s = FindOrAdd(rec.owner);
			++s->count;
			s->busyNs += durNs;
			if (waitMs > s->worstWaitMs) s->worstWaitMs = waitMs;
			s->wait[s->waitSamples % kWaitWindow] = waitMs;
			++s->waitSamples;

			if (rec.flags & kFlagInline)
				++mPool.inlineFallbacks;

			if (!mPerJob)
				return;   // kill switch: counters only, no per-job record path

			++s->frameCount;
			s->frameBusyNs += durNs;
			if (waitMs > s->frameWorstMs) s->frameWorstMs = waitMs;

			if (!mRaw.empty())
			{
				JobRecordPacked& p = mRaw[mRawTotal % mRaw.size()];
				p.startNs    = rec.startNs;
				p.durNs      = ClampU32(durNs);
				p.owner      = rec.owner;
				p.tag        = rec.tag;
				p.range      = rec.range;
				p.rangeCount = rec.rangeCount;
				p.worker     = rec.worker;
				p.flags      = rec.flags;
				p.waitUs     = ClampU32(waitNs / 1000u);
				++mRawTotal;
			}
		}

		void JobTimingTable::EndFrame(uint32_t frame)
		{
			for (uint32_t i = 0; i < mOwnerCount; ++i)
			{
				OwnerSlot& s = mOwners[i];
				if (s.frameCount == 0)
					continue;
				if (mPerJob && !mAgg.empty())
				{
					JobFrameAggregate& a = mAgg[mAggTotal % mAgg.size()];
					a.frame       = frame;
					a.owner       = s.owner;
					a.count       = s.frameCount;
					a.busyMs      = static_cast<float>(s.frameBusyNs) * 1e-6f;
					a.worstWaitMs = s.frameWorstMs;
					++mAggTotal;
				}
				s.frameCount = 0;
				s.frameBusyNs = 0;
				s.frameWorstMs = 0.0f;
			}
		}

		uint32_t JobTimingTable::Drain(Dia::Observation::Job::JobInbox& inbox, uint32_t frame)
		{
			Dia::Threading::JobTimingRecord batch[kDrainBatch];
			uint32_t total = 0;
			for (uint32_t b = 0; b < kMaxBatchesPerDrain; ++b)
			{
				const size_t n = inbox.Drain(batch, kDrainBatch);
				for (size_t i = 0; i < n; ++i)
					Record(batch[i]);
				total += static_cast<uint32_t>(n);
				if (n < kDrainBatch)
					break;
			}
			mPool.inboxDrops = ClampU32(inbox.DroppedCount());
			EndFrame(frame);
			return total;
		}

		void JobTimingTable::NotePoolSample(uint32_t queueDepth, uint32_t activeWorkers, uint32_t workerCount, float dtMs)
		{
			if (queueDepth > mPool.peakQueueDepth) mPool.peakQueueDepth = queueDepth;
			if (activeWorkers > mPool.capUse) mPool.capUse = activeWorkers;
			if (workerCount > 0 && activeWorkers >= workerCount)
				mPool.timeAtCapMs += dtMs;
		}

		float JobTimingTable::Percentile(const OwnerSlot& s, float p) const
		{
			const uint32_t n = s.waitSamples < kWaitWindow ? s.waitSamples : kWaitWindow;
			if (n == 0)
				return 0.0f;
			float tmp[kWaitWindow];
			std::memcpy(tmp, s.wait, n * sizeof(float));
			std::sort(tmp, tmp + n);
			// Nearest rank.
			uint32_t rank = static_cast<uint32_t>(std::ceil(p * static_cast<float>(n)));
			if (rank < 1) rank = 1;
			if (rank > n) rank = n;
			return tmp[rank - 1];
		}

		JobOwnerTier1 JobTimingTable::GetOwner(uint32_t index) const
		{
			JobOwnerTier1 out;
			std::memset(&out, 0, sizeof(out));
			if (index >= mOwnerCount)
				return out;
			const OwnerSlot& s = mOwners[index];
			out.owner       = s.owner;
			out.count       = s.count;
			out.busyNs      = s.busyNs;
			out.waitP50Ms   = Percentile(s, 0.50f);
			out.waitP95Ms   = Percentile(s, 0.95f);
			out.worstWaitMs = s.worstWaitMs;
			return out;
		}

		bool JobTimingTable::GetAggregate(uint64_t index, JobFrameAggregate& out) const
		{
			if (index >= mAggTotal || index < GetAggregateOldest())
				return false;
			out = mAgg[index % mAgg.size()];
			return true;
		}

		bool JobTimingTable::GetRaw(uint64_t index, JobRecordPacked& out) const
		{
			if (index >= mRawTotal || index < GetRawOldest())
				return false;
			out = mRaw[index % mRaw.size()];
			return true;
		}

		uint32_t JobTimingTable::CopyRaw(uint64_t fromNs, uint64_t toNs, JobRecordPacked* out, uint32_t max, bool* truncated) const
		{
			if (truncated) *truncated = false;
			uint32_t n = 0;
			for (uint64_t i = GetRawOldest(); i < mRawTotal; ++i)
			{
				const JobRecordPacked& p = mRaw[i % mRaw.size()];
				if (p.startNs > toNs || p.startNs + p.durNs < fromNs)
					continue;
				if (n >= max)
				{
					if (truncated) *truncated = true;
					break;
				}
				out[n++] = p;
			}
			return n;
		}

		void JobTimingTable::AddStepSummary(const StepSummary& s)
		{
			mSteps[mStepTotal % kStepCapacity] = s;
			++mStepTotal;
		}

		bool JobTimingTable::GetStepSummary(uint32_t indexFromOldest, StepSummary& out) const
		{
			const uint32_t count = GetStepSummaryCount();
			if (indexFromOldest >= count)
				return false;
			const uint64_t first = mStepTotal - count;
			out = mSteps[(first + indexFromOldest) % kStepCapacity];
			return true;
		}
	}}
}
