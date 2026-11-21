#pragma once

#include <DiaObservation/Profile/JobTimingTable.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		class CoarseRing;

		// Same shape as TripNameResolver: writes/returns a name for the crc, nullptr or a
		// resolver that returns nullptr falls back to "0x%08x".
		typedef const char* (*PerfNameResolver)(uint32_t crc, char* buf, size_t bufSize);

		struct PerfWindowUnitInput
		{
			uint32_t          unitCrc;
			const char*       name;
			float             unitHz;
			const CoarseRing* ring;     // may be nullptr (packed as an empty unit)
		};

		struct PerfFrameSection
		{
			const char* name;
			float       ms;
		};

		// perf.window payload. Timing arrays are base64 of little-endian uint16 (0.01 ms fixed
		// point), one JSON string per array; moduleMs is module-major. seconds is clamped to [1, 60].
		void PackPerfWindow(const PerfWindowUnitInput* units, uint32_t unitCount, float seconds,
			PerfNameResolver resolver, std::string& outJson);

		// perf.frame payloads.
		void PackPerfFrameUnavailable(std::string& outJson);
		void PackPerfFrameSections(const char* unit, uint64_t frame, const PerfFrameSection* sections,
			uint32_t count, std::string& outJson);

		// perf.frame payload from the coarse ring alone: unit tick and per-module ms for the entry at
		// absolute index 'frame' (the same index as a window's firstEntry + i), plus an honest
		// sections.available=false (no per-frame section stream exists on Main yet). Unused module
		// slots (crc 0) are skipped. Writes {"error":"..."} when the ring is null or frame is outside
		// the retained range.
		void PackPerfFrame(const PerfWindowUnitInput& unit, uint64_t frame, PerfNameResolver resolver,
			std::string& outJson);

		// ---- Jobs and workers (Perf Inspector task 13) ----

		// One unit's tier 2 input: the unit's per-owner per-frame aggregates (oldest first).
		struct PerfJobsUnitInput
		{
			uint32_t                 unitCrc;
			const char*              name;
			float                    unitHz;            // <= 0 falls back to 60
			bool                     perJobRecording;   // false: aggregates are not produced (kill switch)
			const JobFrameAggregate* aggs;
			uint32_t                 aggCount;
			const StepSummary*       steps = nullptr;   // deferred-step summaries, oldest first (optional)
			uint32_t                 stepCount = 0;
		};

		// Aggregates are bucketed so 60 s of 5 owners fits the AC13 budget: kJobsBucketMs wide buckets
		// (frames per bucket = round(hz * 0.2), at least 1).
		static constexpr uint32_t kJobsBucketMs = 200;

		// The "jobs" section of perf.window, a JSON object:
		//   {"bucketMs":200,"perJobRecording":b,"units":[{"unit","unitCrc","perJobRecording","hz",
		//    "firstFrame","framesPerBucket","bucketCount","owners":[{"crc","name"}],
		//    "frameAgg":{"count":b64,"busyMs":b64,"worstWaitMs":b64}}],"steps":[]}
		// frameAgg arrays are base64 little-endian uint16, owner-major (owner * bucketCount + bucket):
		// count saturates at 65535, busyMs and worstWaitMs are 0.01 ms fixed point saturating at 655.35.
		// A unit with perJobRecording false (or no aggregates) packs no owners and empty arrays.
		// "steps": every unit's step summaries (unit order, oldest first), each
		//   {"unit","channel","kickKey","applyKey","kick","firstStart","ready","apply" (ns, steady clock),
		//    "stallMs","cpuMs","capWaitMs","workerWaitMs","ranges"}; channel is the resolver name or 0x%08x.
		//   Empty when no unit supplied summaries.
		void PackPerfWindowJobs(const PerfJobsUnitInput* units, uint32_t unitCount, float seconds,
			PerfNameResolver resolver, std::string& outJson);

		// perf.jobs payload: raw records as packed arrays, one base64 string per field, record-major order
		// identical across arrays: startNs (u64), durNs/owner/tag/waitUs (u32), worker/range/rangeCount/flags
		// (u16), all little-endian. End = startNs + durNs.
		void PackPerfJobsRecords(const char* unit, uint32_t unitCrc, uint64_t fromNs, uint64_t toNs,
			const JobRecordPacked* records, uint32_t count, bool truncated, bool perJobRecording,
			std::string& outJson);
	}}
}
