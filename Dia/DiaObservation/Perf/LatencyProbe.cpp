#include "LatencyProbe.h"

#include <DiaCore/Json/external/json/json.h>
#include <algorithm>
#include <chrono>

namespace Dia
{
	namespace Observation
	{
		namespace Perf
		{
			namespace
			{
				const uint32_t kIndexBits = 8;
				const uint32_t kIndexMask = (1u << kIndexBits) - 1u;
				const uint32_t kReserved  = 0xFFFFFFFFu;

				uint64_t SteadyNowNs(void*)
				{
					return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
						std::chrono::steady_clock::now().time_since_epoch()).count());
				}
			}

			LatencyProbe::LatencyProbe(NowNsFn nowFn, void* ctx)
				: mNowFn(nowFn ? nowFn : &SteadyNowNs)
				, mCtx(ctx)
			{
				for (unsigned int i = 0; i < kMaxOpenProbes; ++i)
					for (unsigned int h = 0; h < kMaxHops; ++h)
					{
						mOpen[i].hopCrc[h].store(0, std::memory_order_relaxed);
						mOpen[i].hopNs[h].store(0, std::memory_order_relaxed);
					}
				for (unsigned int a = 0; a < kMaxActions; ++a)
					for (unsigned int s = 0; s < kRingSize; ++s)
						for (unsigned int w = 0; w < kWords; ++w)
							mActions[a].slots[s].words[w].store(0, std::memory_order_relaxed);
			}

			uint64_t LatencyProbe::Now() const { return mNowFn(mCtx); }

			uint32_t LatencyProbe::Begin(Dia::Core::StringCRC actionCrc)
			{
				const uint32_t start = mBeginCursor.fetch_add(1, std::memory_order_relaxed);
				for (unsigned int n = 0; n < kMaxOpenProbes; ++n)
				{
					const unsigned int idx = (start + n) % kMaxOpenProbes;
					OpenSlot& s = mOpen[idx];
					uint32_t expected = 0;
					// Reserve with a sentinel so no Stamp/End can match while the slot is being filled.
					if (!s.tag.compare_exchange_strong(expected, kReserved, std::memory_order_acquire))
						continue;
					const uint32_t gen = (s.generation.load(std::memory_order_relaxed) + 1u) & 0x00FFFFFFu;
					const uint32_t genNonZero = gen == 0 ? 1u : gen;
					s.generation.store(genNonZero, std::memory_order_relaxed);
					s.action.store(actionCrc.Value(), std::memory_order_relaxed);
					s.hopCount.store(0, std::memory_order_relaxed);
					s.beginNs.store(Now(), std::memory_order_relaxed);
					const uint32_t id = (genNonZero << kIndexBits) | (idx + 1u);
					s.tag.store(id, std::memory_order_release);
					return id;
				}
				mTableFull.fetch_add(1, std::memory_order_relaxed);
				return 0;
			}

			void LatencyProbe::Stamp(uint32_t id, Dia::Core::StringCRC hopCrc)
			{
				const uint32_t idx = (id & kIndexMask);
				if (id == 0 || idx == 0 || idx > kMaxOpenProbes)
				{
					mRejected.fetch_add(1, std::memory_order_relaxed);
					return;
				}
				OpenSlot& s = mOpen[idx - 1];
				if (s.tag.load(std::memory_order_acquire) != id)
				{
					mRejected.fetch_add(1, std::memory_order_relaxed);
					return;
				}
				const uint64_t now = Now();
				const uint32_t n = s.hopCount.fetch_add(1, std::memory_order_relaxed);
				if (n >= kMaxHops)
				{
					mHopOverflow.fetch_add(1, std::memory_order_relaxed);
					return;
				}
				s.hopCrc[n].store(hopCrc.Value(), std::memory_order_relaxed);
				s.hopNs[n].store(now, std::memory_order_release);
			}

			LatencyProbe::ActionRing* LatencyProbe::FindAction(uint32_t crc) const
			{
				for (unsigned int a = 0; a < kMaxActions; ++a)
					if (mActions[a].crc.load(std::memory_order_acquire) == crc)
						return &mActions[a];
				return nullptr;
			}

			LatencyProbe::ActionRing* LatencyProbe::FindOrClaimAction(uint32_t crc)
			{
				for (unsigned int a = 0; a < kMaxActions; ++a)
				{
					uint32_t cur = mActions[a].crc.load(std::memory_order_acquire);
					if (cur == crc)
						return &mActions[a];
					if (cur == 0)
					{
						uint32_t expected = 0;
						if (mActions[a].crc.compare_exchange_strong(expected, crc, std::memory_order_acq_rel) || expected == crc)
							return &mActions[a];
					}
				}
				return nullptr;
			}

			void LatencyProbe::End(uint32_t id)
			{
				const uint32_t idx = (id & kIndexMask);
				if (id == 0 || idx == 0 || idx > kMaxOpenProbes)
				{
					mRejected.fetch_add(1, std::memory_order_relaxed);
					return;
				}
				OpenSlot& s = mOpen[idx - 1];
				const uint64_t now = Now();
				// Snapshot the data first (the tag still guards the slot), then claim by CAS id -> reserved.
				uint32_t expected = id;
				if (!s.tag.compare_exchange_strong(expected, kReserved, std::memory_order_acq_rel))
				{
					mRejected.fetch_add(1, std::memory_order_relaxed);
					return;
				}

				const uint64_t begin = s.beginNs.load(std::memory_order_relaxed);
				uint32_t hops = s.hopCount.load(std::memory_order_relaxed);
				if (hops > kMaxHops) hops = kMaxHops;
				uint64_t words[kWords];
				words[0] = now >= begin ? now - begin : 0;
				words[1] = hops;
				uint64_t prev = begin;
				for (uint32_t h = 0; h < kMaxHops; ++h)
				{
					uint32_t crc = 0; uint64_t since = 0, sincePrev = 0;
					if (h < hops)
					{
						crc = s.hopCrc[h].load(std::memory_order_relaxed);
						const uint64_t ns = s.hopNs[h].load(std::memory_order_acquire);
						since     = ns >= begin ? ns - begin : 0;
						sincePrev = ns >= prev  ? ns - prev  : 0;
						prev = ns;
					}
					words[2 + h * 3 + 0] = crc;
					words[2 + h * 3 + 1] = since;
					words[2 + h * 3 + 2] = sincePrev;
				}
				const uint32_t actionCrc = s.action.load(std::memory_order_relaxed);
				s.tag.store(0, std::memory_order_release);   // slot free again

				ActionRing* ring = FindOrClaimAction(actionCrc);
				if (!ring)
				{
					mCompletionsDropped.fetch_add(1, std::memory_order_relaxed);
					return;
				}
				const uint32_t seq = ring->writeSeq.fetch_add(1, std::memory_order_acq_rel);
				RingSlot& slot = ring->slots[seq % kRingSize];
				uint32_t v = slot.version.load(std::memory_order_acquire);
				if ((v & 1u) != 0 || !slot.version.compare_exchange_strong(v, v + 1, std::memory_order_acq_rel))
				{
					mCompletionsDropped.fetch_add(1, std::memory_order_relaxed);   // another writer owns this slot
					return;
				}
				for (unsigned int w = 0; w < kWords; ++w)
					slot.words[w].store(words[w], std::memory_order_relaxed);
				slot.version.store(v + 2, std::memory_order_release);
			}

			bool LatencyProbe::ReadSlot(const RingSlot& s, CompletedProbe& out)
			{
				for (int attempt = 0; attempt < 4; ++attempt)
				{
					const uint32_t v1 = s.version.load(std::memory_order_acquire);
					if ((v1 & 1u) != 0 || v1 == 0)
						continue;
					uint64_t w[kWords];
					for (unsigned int i = 0; i < kWords; ++i)
						w[i] = s.words[i].load(std::memory_order_relaxed);
					std::atomic_thread_fence(std::memory_order_acquire);
					if (s.version.load(std::memory_order_relaxed) != v1)
						continue;
					out.totalNs  = w[0];
					out.hopCount = static_cast<uint32_t>(w[1]);
					for (unsigned int h = 0; h < kMaxHops; ++h)
					{
						out.hops[h].hopCrc       = static_cast<uint32_t>(w[2 + h * 3]);
						out.hops[h].sinceBeginNs = w[2 + h * 3 + 1];
						out.hops[h].sincePrevNs  = w[2 + h * 3 + 2];
					}
					return true;
				}
				return false;
			}

			unsigned int LatencyProbe::GetCompletions(Dia::Core::StringCRC actionCrc, CompletedProbe* out, unsigned int maxOut) const
			{
				const ActionRing* ring = FindAction(actionCrc.Value());
				if (!ring || !out || maxOut == 0)
					return 0;
				const uint32_t written = ring->writeSeq.load(std::memory_order_acquire);
				const uint32_t count = std::min<uint32_t>(written, kRingSize);
				unsigned int n = 0;
				for (uint32_t i = 0; i < count && n < maxOut; ++i)
				{
					const uint32_t seq = written - count + i;
					if (ReadSlot(ring->slots[seq % kRingSize], out[n]))
						++n;
				}
				return n;
			}

			LatencyStats LatencyProbe::GetStats(Dia::Core::StringCRC actionCrc) const
			{
				LatencyStats stats;
				CompletedProbe items[kRingSize];
				const unsigned int n = GetCompletions(actionCrc, items, kRingSize);
				if (n == 0)
					return stats;
				uint64_t totals[kRingSize];
				for (unsigned int i = 0; i < n; ++i)
					totals[i] = items[i].totalNs;
				std::sort(totals, totals + n);
				// Nearest rank: rank = ceil(p * n) (integer math), value = sorted[rank - 1].
				const unsigned int r50 = (50u * n + 99u) / 100u;
				const unsigned int r95 = (95u * n + 99u) / 100u;
				stats.count = n;
				stats.p50Ns = totals[(r50 == 0 ? 1 : r50) - 1];
				stats.p95Ns = totals[(r95 == 0 ? 1 : r95) - 1];
				stats.minNs = totals[0];
				stats.maxNs = totals[n - 1];
				return stats;
			}

			LatencyCounters LatencyProbe::GetCounters() const
			{
				LatencyCounters c;
				c.rejected           = mRejected.load(std::memory_order_relaxed);
				c.tableFull          = mTableFull.load(std::memory_order_relaxed);
				c.hopOverflow        = mHopOverflow.load(std::memory_order_relaxed);
				c.completionsDropped = mCompletionsDropped.load(std::memory_order_relaxed);
				return c;
			}

			bool LatencyVisibleMatcher::NoteApplied(uint32_t id, uint64_t simFrame)
			{
				if (id == 0)
					return false;
				mProbe.Stamp(id, LatencyHops::Applied());
				for (unsigned int i = 0; i < kCapacity; ++i)
				{
					uint32_t expected = 0;
					// Reserve, publish the frame, then publish the id (OnSnapshot ignores the sentinel).
					if (mEntries[i].id.compare_exchange_strong(expected, kReserved, std::memory_order_acq_rel))
					{
						mEntries[i].frame.store(simFrame, std::memory_order_relaxed);
						mEntries[i].id.store(id, std::memory_order_release);
						return true;
					}
				}
				return false;
			}

			unsigned int LatencyVisibleMatcher::OnSnapshot(uint64_t simFrame)
			{
				unsigned int completed = 0;
				for (unsigned int i = 0; i < kCapacity; ++i)
				{
					uint32_t id = mEntries[i].id.load(std::memory_order_acquire);
					if (id == 0 || id == kReserved)
						continue;
					if (mEntries[i].frame.load(std::memory_order_relaxed) > simFrame)
						continue;
					if (!mEntries[i].id.compare_exchange_strong(id, 0, std::memory_order_acq_rel))
						continue;
					mProbe.Stamp(id, LatencyHops::Visible());
					mProbe.End(id);
					++completed;
				}
				return completed;
			}

			bool StripLatencyArg(Json::Value& args, uint32_t& outId)
			{
				outId = 0;
				if (!args.isObject() || !args.isMember("_lat"))
					return false;
				const Json::Value lat = args["_lat"];
				args.removeMember("_lat");
				if (!lat.isNumeric())
					return false;
				const double d = lat.asDouble();
				if (d <= 0.0 || d > 4294967295.0)
					return false;
				outId = lat.asUInt();
				return outId != 0;
			}
		}
	}
}
