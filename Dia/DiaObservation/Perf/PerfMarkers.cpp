#include "DiaObservation/Perf/PerfMarkers.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Dia
{
	namespace Observation
	{
		namespace Perf
		{
			namespace
			{
				// Per-slot seqlock. Slot for marker index n: seq == 2n+1 while writing, 2n+2 when complete.
				struct Slot
				{
					std::atomic<uint64_t> seq{0};
					std::atomic<uint32_t> nameCrc{0};
					std::atomic<uint32_t> hasValue{0};
					std::atomic<uint64_t> value{0};
					std::atomic<uint64_t> steadyNs{0};
				};

				struct Ring
				{
					Slot                  slots[PerfMarkers::kRingCapacity];
					std::atomic<uint64_t> head{0};      // total markers written (single producer)
					std::atomic<uint64_t> dropped{0};   // overwritten-before-read count
					uint32_t              threadId = 0;
				};

				thread_local Ring tRing;
				thread_local bool tRegistered = false;

				std::mutex            sMutex;                      // register/unregister/snapshot only
				Ring*                 sRings[PerfMarkers::kMaxThreads] = {};
				unsigned int          sRingCount = 0;
				std::atomic<uint64_t> sUnregisteredDropped{0};    // marks from unregistered threads + retired ring drops

				uint32_t CurrentThreadId()
				{
#ifdef _WIN32
					return static_cast<uint32_t>(GetCurrentThreadId());
#else
					return 0;
#endif
				}

				uint64_t NowNs()
				{
					return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
				}

				void Write(uint32_t nameCrc, bool hasValue, uint64_t value)
				{
					if (!tRegistered)
					{
						sUnregisteredDropped.fetch_add(1, std::memory_order_relaxed);
						return;
					}

					Ring& r = tRing;
					const uint64_t n = r.head.load(std::memory_order_relaxed);
					if (n >= PerfMarkers::kRingCapacity)
						r.dropped.fetch_add(1, std::memory_order_relaxed);   // overwriting the oldest

					Slot& s = r.slots[n % PerfMarkers::kRingCapacity];
					s.seq.store(2 * n + 1, std::memory_order_relaxed);
					std::atomic_thread_fence(std::memory_order_release);
					s.nameCrc.store(nameCrc, std::memory_order_relaxed);
					s.hasValue.store(hasValue ? 1u : 0u, std::memory_order_relaxed);
					s.value.store(value, std::memory_order_relaxed);
					s.steadyNs.store(NowNs(), std::memory_order_relaxed);
					s.seq.store(2 * n + 2, std::memory_order_release);
					r.head.store(n + 1, std::memory_order_release);
				}
			}

			void PerfMarkers::RegisterThread()
			{
				if (tRegistered)
					return;

				std::lock_guard<std::mutex> lock(sMutex);
				if (sRingCount >= kMaxThreads)
					return;   // stays unregistered: marks are dropped and counted

				Ring& r = tRing;
				for (unsigned int i = 0; i < kRingCapacity; ++i)
					r.slots[i].seq.store(0, std::memory_order_relaxed);
				r.head.store(0, std::memory_order_relaxed);
				r.dropped.store(0, std::memory_order_relaxed);
				r.threadId = CurrentThreadId();

				sRings[sRingCount++] = &r;
				tRegistered = true;
			}

			void PerfMarkers::UnregisterThread()
			{
				if (!tRegistered)
					return;

				std::lock_guard<std::mutex> lock(sMutex);
				for (unsigned int i = 0; i < sRingCount; ++i)
				{
					if (sRings[i] == &tRing)
					{
						sRings[i] = sRings[sRingCount - 1];
						sRings[sRingCount - 1] = nullptr;
						--sRingCount;
						break;
					}
				}
				sUnregisteredDropped.fetch_add(tRing.dropped.load(std::memory_order_relaxed), std::memory_order_relaxed);
				tRing.dropped.store(0, std::memory_order_relaxed);
				tRing.head.store(0, std::memory_order_relaxed);
				tRegistered = false;
			}

			bool PerfMarkers::IsThreadRegistered()
			{
				return tRegistered;
			}

			void PerfMarkers::Mark(uint32_t nameCrc)
			{
				Write(nameCrc, false, 0);
			}

			void PerfMarkers::MarkValue(uint32_t nameCrc, uint64_t value)
			{
				Write(nameCrc, true, value);
			}

			unsigned int PerfMarkers::Snapshot(PerfMarker* out, unsigned int maxOut, uint64_t* outDropped)
			{
				std::vector<PerfMarker> all;
				uint64_t dropped = sUnregisteredDropped.load(std::memory_order_relaxed);

				{
					std::lock_guard<std::mutex> lock(sMutex);
					all.reserve(static_cast<size_t>(sRingCount) * kRingCapacity);
					for (unsigned int t = 0; t < sRingCount; ++t)
					{
						const Ring& r = *sRings[t];
						dropped += r.dropped.load(std::memory_order_relaxed);

						const uint64_t head  = r.head.load(std::memory_order_acquire);
						const uint64_t first = head > kRingCapacity ? head - kRingCapacity : 0;
						for (uint64_t n = first; n < head; ++n)
						{
							const Slot& s = r.slots[n % kRingCapacity];
							const uint64_t expect = 2 * n + 2;
							if (s.seq.load(std::memory_order_acquire) != expect)
								continue;   // being overwritten or already overwritten

							PerfMarker m;
							m.nameCrc  = s.nameCrc.load(std::memory_order_relaxed);
							m.hasValue = s.hasValue.load(std::memory_order_relaxed) != 0;
							m.value    = s.value.load(std::memory_order_relaxed);
							m.steadyNs = s.steadyNs.load(std::memory_order_relaxed);
							m.threadId = r.threadId;
							std::atomic_thread_fence(std::memory_order_acquire);
							if (s.seq.load(std::memory_order_relaxed) != expect)
								continue;   // torn: producer overwrote during the copy
							all.push_back(m);
						}
					}
				}

				std::stable_sort(all.begin(), all.end(),
					[](const PerfMarker& a, const PerfMarker& b) { return a.steadyNs < b.steadyNs; });

				size_t count = all.size();
				size_t skip  = 0;
				if (count > maxOut) { skip = count - maxOut; count = maxOut; }
				for (size_t i = 0; i < count; ++i)
					out[i] = all[skip + i];

				if (outDropped)
					*outDropped = dropped;
				return static_cast<unsigned int>(count);
			}

			uint64_t PerfMarkers::TotalDropped()
			{
				uint64_t dropped = 0;
				Snapshot(nullptr, 0, &dropped);
				return dropped;
			}
		}
	}
}
