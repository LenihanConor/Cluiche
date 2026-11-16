#pragma once

// Perf event markers (perf-live-control Goal 2).
//
// A per-thread lock-free ring of 1,024 markers. The owning thread is the single producer; a reader
// on another thread (the DebugServer bridge) takes snapshots through a per-slot seqlock, so a torn
// entry is never returned. No allocation, no I/O and no lock on the mark path. When the ring is full
// the OLDEST marker is overwritten and a per-thread drop counter is incremented. A thread that has
// not called RegisterThread() has its marks dropped and counted.

#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia
{
	namespace Observation
	{
		namespace Perf
		{
			struct PerfMarker
			{
				uint32_t nameCrc;
				uint32_t threadId;
				uint64_t value;       // valid only when hasValue
				uint64_t steadyNs;    // std::chrono::steady_clock, same base as ScopedZone
				bool     hasValue;
			};

			class PerfMarkers
			{
			public:
				static const unsigned int kRingCapacity = 1024;
				static const unsigned int kMaxThreads   = 16;

				// Register / unregister the calling thread's ring (no-op when already registered).
				// Registration beyond kMaxThreads leaves the thread unregistered (its marks are dropped).
				// A registered thread must unregister before it exits.
				static void RegisterThread();
				static void UnregisterThread();
				static bool IsThreadRegistered();

				// Hot path. Never allocates, never blocks.
				static void Mark(uint32_t nameCrc);
				static void MarkValue(uint32_t nameCrc, uint64_t value);

				// Copies up to maxOut of the NEWEST markers across all registered threads, ordered
				// oldest to newest by steadyNs. Non-consuming. outDropped (optional) receives the
				// total drops: ring overwrites + unregistered-thread marks. Safe from any thread.
				static unsigned int Snapshot(PerfMarker* out, unsigned int maxOut, uint64_t* outDropped = nullptr);

				static uint64_t TotalDropped();
			};
		}
	}
}

// Name must be a string literal; its CRC is computed once per call site.
#define DIA_PERF_MARK_CRC_(name, var) \
	static const uint32_t var = ::Dia::Core::StringCRC(name).Value()

#define DIA_PERF_MARK(name) \
	do { \
		DIA_PERF_MARK_CRC_(name, _dia_perf_crc_); \
		::Dia::Observation::Perf::PerfMarkers::Mark(_dia_perf_crc_); \
	} while (0)

#define DIA_PERF_MARK_VALUE(name, value) \
	do { \
		DIA_PERF_MARK_CRC_(name, _dia_perf_crc_); \
		::Dia::Observation::Perf::PerfMarkers::MarkValue(_dia_perf_crc_, static_cast<uint64_t>(value)); \
	} while (0)
