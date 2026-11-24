#pragma once

#include <DiaObservation/Job/JobInbox.h>
#include <DiaThreading/JobContext.h>

#include <cstdint>
#include <vector>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Perf Inspector task 13 (jobs and workers). Plain data types shared by JobTimingTable,
		// JobChunk and the Main-side assembler. All trivially copyable.

		// Tier 1: cumulative per-owner counters. owner == 0 in the last slot is the overflow bucket.
		struct JobOwnerTier1
		{
			uint32_t owner;
			uint32_t count;          // jobs finished (cumulative)
			uint64_t busyNs;         // sum of (end - start) (cumulative)
			float    waitP50Ms;      // queue wait (start - submit), short window
			float    waitP95Ms;
			float    worstWaitMs;    // worst queue wait seen (cumulative)
			uint32_t reserved;
		};

		// Tier 1: pool counters. Fields the job observation does not expose stay 0 until a sampler
		// calls JobTimingTable::NotePoolSample (nothing does yet).
		struct JobPoolCounters
		{
			uint32_t capUse;            // peak active workers seen (NotePoolSample)
			uint32_t peakQueueDepth;    // NotePoolSample
			uint32_t inlineFallbacks;   // records carrying kFlagInline
			uint32_t inboxDrops;        // JobInbox::DroppedCount (truncated to 32 bits)
			float    timeAtCapMs;       // NotePoolSample
			uint32_t reserved;
		};

		// Tier 2: one aggregate per owner per drained frame (per-job recording only).
		struct JobFrameAggregate
		{
			uint32_t frame;
			uint32_t owner;
			uint32_t count;
			float    busyMs;
			float    worstWaitMs;
		};

		// Tier 3: one raw job (per-job recording only). 32 bytes.
		struct JobRecordPacked
		{
			uint64_t startNs;
			uint32_t durNs;       // clamped to ~4.29 s
			uint32_t owner;
			uint32_t tag;
			uint16_t range;
			uint16_t rangeCount;
			uint16_t worker;
			uint16_t flags;
			uint32_t waitUs;      // start - submit, clamped
		};
		static_assert(sizeof(JobRecordPacked) == 32, "JobRecordPacked must be 32 bytes");

		// Tier 2 deferred-step summary (hook only: no producer exists yet; DeferredStepModule
		// Feature 4 fills it via AddStepSummary once built).
		struct StepSummary
		{
			uint32_t channel;
			uint32_t kickKey;
			uint32_t applyKey;
			uint16_t ranges;
			uint16_t reserved;
			uint64_t kickNs;
			uint64_t firstStartNs;
			uint64_t readyNs;
			uint64_t applyNs;
			float    stallMs;
			float    cpuMs;
			float    capWaitMs;
			float    workerWaitMs;
		};

		// Per-ProcessingUnit job timing table. Pure data: no clock reads, no allocation after Init().
		// One PU thread writes; nobody else reads it (publication is by JobChunk through a stream).
		class JobTimingTable
		{
		public:
			static constexpr uint32_t kMaxOwners          = 16;
			static constexpr uint32_t kWaitWindow         = 256;    // queue-wait samples kept per owner
			static constexpr uint32_t kDefaultRawCapacity = 8192;   // ~5 s at 1,600 jobs/s
			static constexpr uint32_t kAggregateCapacity  = 1024;
			static constexpr uint32_t kStepCapacity       = 64;
			static constexpr uint32_t kDrainBatch         = 64;
			static constexpr uint32_t kMaxBatchesPerDrain = 32;
			static constexpr uint16_t kFlagInline         = 1;      // JobTimingRecord::flags bit: ran inline

			JobTimingTable();

			// Allocates the rings and clears all state. Safe to call again (reallocates).
			void Init(uint32_t rawCapacity = kDefaultRawCapacity);
			void Reset();

			// Kill switch (observation.perf.jobs): off = tier 1 counters only; no raw ring, no aggregates.
			void SetPerJobRecording(bool on) { mPerJob = on; }
			bool GetPerJobRecording() const  { return mPerJob; }

			// Hot path. O(records), no allocation.
			void Record(const Dia::Threading::JobTimingRecord& rec);
			void EndFrame(uint32_t frame);                                           // closes the per-owner per-frame aggregates
			uint32_t Drain(Dia::Observation::Job::JobInbox& inbox, uint32_t frame);  // Record all + EndFrame; returns records consumed

			// Optional pool sampler (nothing calls it yet).
			void NotePoolSample(uint32_t queueDepth, uint32_t activeWorkers, uint32_t workerCount, float dtMs);

			// --- Tier 1 ---
			uint32_t      GetOwnerCount() const { return mOwnerCount; }
			JobOwnerTier1 GetOwner(uint32_t index) const;   // percentiles computed on call (off the hot path)
			const JobPoolCounters& GetPool() const { return mPool; }
			uint64_t      GetRecordsSeen() const { return mRecordsSeen; }

			// --- Tier 2 aggregates (absolute, monotonically increasing indexes) ---
			uint64_t GetAggregateTotal() const { return mAggTotal; }
			uint64_t GetAggregateOldest() const { return mAggTotal > mAgg.size() ? mAggTotal - mAgg.size() : 0; }
			bool     GetAggregate(uint64_t index, JobFrameAggregate& out) const;

			// --- Tier 3 raw ring ---
			uint64_t GetRawTotal() const { return mRawTotal; }
			uint64_t GetRawOldest() const { return mRawTotal > mRaw.size() ? mRawTotal - mRaw.size() : 0; }
			uint64_t GetRawOverwritten() const { return GetRawOldest(); }
			uint32_t GetRawCount() const { return static_cast<uint32_t>(mRawTotal - GetRawOldest()); }
			uint32_t GetRawCapacity() const { return static_cast<uint32_t>(mRaw.size()); }
			bool     GetRaw(uint64_t index, JobRecordPacked& out) const;
			// Records overlapping [fromNs, toNs], oldest first; truncated set when more than max matched.
			uint32_t CopyRaw(uint64_t fromNs, uint64_t toNs, JobRecordPacked* out, uint32_t max, bool* truncated) const;

			// --- Tier 2 step summaries (empty hook) ---
			void     AddStepSummary(const StepSummary& s);
			uint32_t GetStepSummaryCount() const { return mStepTotal < kStepCapacity ? static_cast<uint32_t>(mStepTotal) : kStepCapacity; }
			uint64_t GetStepSummaryTotal() const { return mStepTotal; }
			bool     GetStepSummary(uint32_t indexFromOldest, StepSummary& out) const;

		private:
			struct OwnerSlot
			{
				uint32_t owner;
				uint32_t count;
				uint64_t busyNs;
				float    worstWaitMs;
				uint32_t waitSamples;                // total recorded
				float    wait[kWaitWindow];
				// current-frame accumulators (per-job recording only)
				uint32_t frameCount;
				uint64_t frameBusyNs;
				float    frameWorstMs;
			};

			OwnerSlot* FindOrAdd(uint32_t owner);
			float      Percentile(const OwnerSlot& s, float p) const;

			OwnerSlot mOwners[kMaxOwners];
			uint32_t  mOwnerCount;
			JobPoolCounters mPool;
			uint64_t  mRecordsSeen;
			bool      mPerJob;

			std::vector<JobRecordPacked>   mRaw;
			uint64_t                       mRawTotal;
			std::vector<JobFrameAggregate> mAgg;
			uint64_t                       mAggTotal;
			StepSummary                    mSteps[kStepCapacity];
			uint64_t                       mStepTotal;
		};
	}}
}
