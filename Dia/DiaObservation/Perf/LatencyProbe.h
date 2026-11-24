#pragma once

// Action-to-visible latency probes (perf-live-control Goal 3).
//
// Begin(action) -> Stamp(id, hop)* -> End(id). Completed probes go into a ring of the last 40 per action;
// GetStats() reports count/min/max/p50/p95 over that ring. Percentile method: NEAREST RANK on the ascending
// sorted totals, rank = ceil(p * n), result = sorted[rank - 1] (so 40 samples: p50 = 20th, p95 = 38th).
//
// THREADING MODEL (no mutex, no allocation after construction, no I/O, never logs):
//  - Begin/Stamp/End may be called from any thread (Main begins, Sim stamps, Main ends).
//  - Open probes live in a fixed table of kMaxOpenProbes slots. Each slot has an atomic "tag" holding the
//    probe id while the slot is open and 0 otherwise. Begin claims a free slot by CAS(0 -> id); End claims
//    the probe by CAS(id -> 0), so exactly one End wins and a double End is rejected. Ids are
//    (generation << 8) | (slotIndex + 1); the per-slot generation is bumped on every Begin, so a stale id
//    can never match a reused slot (it fails the tag compare and counts as rejected).
//  - Hop stamps: Stamp reserves an index with fetch_add on the slot's hop counter and writes a bounded
//    array (kMaxHops); hops past the limit are dropped and counted (hopOverflow). Callers must order
//    Stamp(id) before End(id) for the same probe (happens-before, e.g. the same actor or a hand-off); a
//    Stamp racing a concurrent End of the same probe may be lost.
//  - Completion ring: kMaxActions action entries (claimed by CAS on the action CRC). Each action has a
//    ring of 40 slots; End takes a position with fetch_add on a write counter and writes the slot between
//    two increments of a per-slot seqlock version (relaxed-atomic words, so torn entries are detected, not
//    observed). Readers (GetStats/GetCompletions, normally Main) retry a few times and skip a slot that is
//    still being written. Writers never wait for readers.
//  - If every action entry is taken, further new actions are dropped and counted (completionsDropped).

#include <DiaCore/CRC/StringCRC.h>
#include <atomic>
#include <cstdint>

namespace Json { class Value; }

namespace Dia
{
	namespace Observation
	{
		namespace Perf
		{
			// Hop names shared by the plumbing and the call sites.
			struct LatencyHops
			{
				static const Dia::Core::StringCRC& Applied() { static const Dia::Core::StringCRC c("applied"); return c; }
				static const Dia::Core::StringCRC& Visible() { static const Dia::Core::StringCRC c("visible"); return c; }
			};

			struct LatencyHop
			{
				uint32_t hopCrc;
				uint64_t sinceBeginNs;
				uint64_t sincePrevNs;
			};

			struct CompletedProbe
			{
				uint64_t   totalNs;
				uint32_t   hopCount;
				LatencyHop hops[8];
			};

			struct LatencyStats
			{
				uint32_t count = 0;   // completions in the ring (max 40)
				uint64_t p50Ns = 0;
				uint64_t p95Ns = 0;
				uint64_t minNs = 0;
				uint64_t maxNs = 0;
			};

			struct LatencyCounters
			{
				uint64_t rejected;           // Stamp/End with an unknown, stale or already-ended id
				uint64_t tableFull;          // Begin returned 0
				uint64_t hopOverflow;        // hops dropped past kMaxHops
				uint64_t completionsDropped; // completions lost because the action table was full
			};

			class LatencyProbe
			{
			public:
				static const unsigned int kMaxOpenProbes = 64;
				static const unsigned int kMaxHops       = 8;
				static const unsigned int kMaxActions    = 16;
				static const unsigned int kRingSize      = 40;

				typedef uint64_t (*NowNsFn)(void* ctx);

				// nowFn == nullptr -> std::chrono::steady_clock nanoseconds.
				explicit LatencyProbe(NowNsFn nowFn = nullptr, void* ctx = nullptr);
				LatencyProbe(const LatencyProbe&) = delete;
				LatencyProbe& operator=(const LatencyProbe&) = delete;

				// Returns a probe id, or 0 when the open-probe table is full.
				uint32_t Begin(Dia::Core::StringCRC actionCrc);
				void Stamp(uint32_t id, Dia::Core::StringCRC hopCrc);
				void End(uint32_t id);

				LatencyStats GetStats(Dia::Core::StringCRC actionCrc) const;
				// Copies up to maxOut completions, oldest first. Returns the number copied.
				unsigned int GetCompletions(Dia::Core::StringCRC actionCrc, CompletedProbe* out, unsigned int maxOut) const;
				LatencyCounters GetCounters() const;

			private:
				static const unsigned int kWords = 2 + kMaxHops * 3;

				struct OpenSlot
				{
					std::atomic<uint32_t> tag{0};        // id while open, else 0
					std::atomic<uint32_t> generation{0};
					std::atomic<uint32_t> action{0};
					std::atomic<uint32_t> hopCount{0};
					std::atomic<uint64_t> beginNs{0};
					std::atomic<uint32_t> hopCrc[kMaxHops];
					std::atomic<uint64_t> hopNs[kMaxHops];
				};

				struct RingSlot
				{
					std::atomic<uint32_t> version{0};    // odd while being written
					std::atomic<uint64_t> words[kWords];
				};

				struct ActionRing
				{
					std::atomic<uint32_t> crc{0};        // 0 = free
					std::atomic<uint32_t> writeSeq{0};
					RingSlot              slots[kRingSize];
				};

				uint64_t Now() const;
				ActionRing* FindAction(uint32_t crc) const;
				ActionRing* FindOrClaimAction(uint32_t crc);
				static bool ReadSlot(const RingSlot& s, CompletedProbe& out);

				NowNsFn mNowFn;
				void*   mCtx;
				mutable OpenSlot   mOpen[kMaxOpenProbes];
				mutable ActionRing mActions[kMaxActions];
				std::atomic<uint32_t> mBeginCursor{0};
				std::atomic<uint64_t> mRejected{0};
				std::atomic<uint64_t> mTableFull{0};
				std::atomic<uint64_t> mHopOverflow{0};
				std::atomic<uint64_t> mCompletionsDropped{0};
			};

			// Records "applied at simFrame F" for probes (Sim thread) and completes them on the first snapshot
			// whose simFrame >= F (Main thread). Fixed capacity, lock-free; a full matcher rejects NoteApplied.
			class LatencyVisibleMatcher
			{
			public:
				static const unsigned int kCapacity = 16;

				explicit LatencyVisibleMatcher(LatencyProbe& probe) : mProbe(probe) {}

				// Stamps the "applied" hop and remembers the frame. False when id == 0 or the matcher is full.
				bool NoteApplied(uint32_t id, uint64_t simFrame);
				// Stamps "visible" and ends every pending probe with appliedFrame <= simFrame. Returns the count.
				unsigned int OnSnapshot(uint64_t simFrame);

			private:
				struct Entry
				{
					std::atomic<uint32_t> id{0};
					std::atomic<uint64_t> frame{0};
				};
				LatencyProbe& mProbe;
				Entry mEntries[kCapacity];
			};

			// Removes "_lat" from a panel command's args and returns its probe id. True only when a numeric,
			// non-zero _lat was present. A non-numeric _lat is removed too (and reported false).
			bool StripLatencyArg(Json::Value& args, uint32_t& outId);
		}
	}
}
