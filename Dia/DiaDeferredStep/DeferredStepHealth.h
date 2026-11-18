#pragma once

#include <atomic>
#include <stdint.h>
#include <DiaObservation/Health/HealthReporterBase.h>

namespace Dia::DeferredStep
{
	// IHealthReporter "DeferredStep": kOK, or kDegraded (reason step_stall) for 60 s after the last stall.
	// Report() is lock-free and allocation-free (one atomic read): safe from Main and from the crash handler.
	// NoteStall() is called from SimPU only.
	class DeferredStepHealth final : public Dia::Observation::Health::HealthReporterBase
	{
	public:
		static constexpr uint64_t kStallWindowNs = 60ull * 1000ull * 1000ull * 1000ull;

		Dia::Core::StringCRC GetReporterName() const override;
		Dia::Observation::Health::Health Report() const override;

		// Stamps "a stall just happened" with the current time.
		void NoteStall();

		// TEST ONLY: replaces the steady clock. 0 returns to the real clock. Must be non-zero to be a valid time.
		void SetNowForTest(uint64_t nowNs) { mNowOverrideNs.store(nowNs, std::memory_order_relaxed); }

	private:
		uint64_t NowNs() const;

		std::atomic<uint64_t> mLastStallNs{ 0 };      // 0 = never stalled
		std::atomic<uint64_t> mNowOverrideNs{ 0 };
	};
}
