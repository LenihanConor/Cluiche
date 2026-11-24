#include <DiaDeferredStep/DeferredStepHealth.h>

#include <chrono>

namespace Dia::DeferredStep
{
	using Dia::Observation::Health::Health;
	using Dia::Observation::Health::HealthStatus;

	uint64_t DeferredStepHealth::NowNs() const
	{
		const uint64_t overrideNs = mNowOverrideNs.load(std::memory_order_relaxed);
		if (overrideNs != 0) { return overrideNs; }
		return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	Dia::Core::StringCRC DeferredStepHealth::GetReporterName() const
	{
		return Dia::Core::StringCRC("DeferredStep");
	}

	void DeferredStepHealth::NoteStall()
	{
		mLastStallNs.store(NowNs(), std::memory_order_release);
	}

	Health DeferredStepHealth::Report() const
	{
		Health h;
		h.status = HealthStatus::kOK;
		h.errors = 0;
		h.warnings = 0;

		const uint64_t last = mLastStallNs.load(std::memory_order_acquire);
		if (last != 0 && NowNs() - last < kStallWindowNs)
		{
			// A stall is a PERFORMANCE signal, not a failure: it must never map to kFailing, because kFailing
			// would flip an E2E run's overall_status (user decision 2026-11-13). Degraded is the ceiling.
			h.status = HealthStatus::kDegraded;
			h.reason = Dia::Core::StringCRC("step_stall");
		}
		return h;
	}
}
