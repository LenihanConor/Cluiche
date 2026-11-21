#pragma once

#include <DiaThreading/JobContext.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Dia
{
	namespace Observation
	{
		namespace Job
		{
			// Fixed-capacity lock-free MPSC ring of JobTimingRecord (Vyukov bounded queue,
			// sequence-per-slot). Producers: any worker thread. Consumer: one thread.
			// Full => the record being pushed (newest) is dropped and counted; never blocks.
			// No allocation after construction.
			class JobInbox
			{
			public:
				explicit JobInbox(size_t capacity)
				{
					size_t cap = 2;
					while (cap < capacity) cap <<= 1;
					mCapacity = cap;
					mMask = cap - 1;
					mSlots.reset(new Slot[cap]);
					for (size_t i = 0; i < cap; ++i) mSlots[i].seq.store(i, std::memory_order_relaxed);
				}

				JobInbox(const JobInbox&) = delete;
				JobInbox& operator=(const JobInbox&) = delete;

				bool TryPush(const Dia::Threading::JobTimingRecord& record)
				{
					size_t pos = mTail.load(std::memory_order_relaxed);
					for (;;)
					{
						Slot& slot = mSlots[pos & mMask];
						const size_t seq = slot.seq.load(std::memory_order_acquire);
						const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
						if (diff == 0)
						{
							if (mTail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
							{
								slot.record = record;
								slot.seq.store(pos + 1, std::memory_order_release);
								mPushed.fetch_add(1, std::memory_order_relaxed);
								return true;
							}
						}
						else if (diff < 0)
						{
							mDropped.fetch_add(1, std::memory_order_relaxed);
							return false;
						}
						else
						{
							pos = mTail.load(std::memory_order_relaxed);
						}
					}
				}

				// Single consumer only.
				bool TryPop(Dia::Threading::JobTimingRecord& out)
				{
					const size_t pos = mHead.load(std::memory_order_relaxed);
					Slot& slot = mSlots[pos & mMask];
					const size_t seq = slot.seq.load(std::memory_order_acquire);
					if (static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1) < 0)
						return false;
					out = slot.record;
					slot.seq.store(pos + mCapacity, std::memory_order_release);
					mHead.store(pos + 1, std::memory_order_relaxed);
					return true;
				}

				// Single consumer only. Pops up to max records into out; returns count.
				size_t Drain(Dia::Threading::JobTimingRecord* out, size_t max)
				{
					size_t n = 0;
					while (n < max && TryPop(out[n])) ++n;
					return n;
				}

				uint64_t DroppedCount() const { return mDropped.load(std::memory_order_relaxed); }
				uint64_t PushedCount() const { return mPushed.load(std::memory_order_relaxed); }
				size_t Capacity() const { return mCapacity; }

			private:
				struct Slot
				{
					std::atomic<size_t> seq;
					Dia::Threading::JobTimingRecord record;
				};

				std::unique_ptr<Slot[]> mSlots;
				size_t mCapacity = 0;
				size_t mMask = 0;
				alignas(64) std::atomic<size_t> mTail{0};
				alignas(64) std::atomic<size_t> mHead{0};
				alignas(64) std::atomic<uint64_t> mPushed{0};
				std::atomic<uint64_t> mDropped{0};
			};
		}
	}
}
