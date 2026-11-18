#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <stdint.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/CapGate.h>
#include <DiaDeferredStep/CappedParallelExecutor.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/IStepLauncher.h>

namespace Dia::Threading { class JobSystem; }

namespace Dia::DeferredStep
{
	// Runs each launched step's Run() on a JobSystem worker, with its ParallelFor ranges admitted
	// through one shared CapGate (strict cap M across every channel).
	//
	// THREADING: only the SimPU thread calls Launch / IsComplete / Wait / Release, so the token table
	// has no lock. Worker threads touch only a slot's `complete` flag, its GateItem (through the gate)
	// and its executor, all of which are address-stable for the slot's whole in-use lifetime.
	//
	// Launch never blocks and never fails (AC 14, 16, 17):
	//   - gate has a free slot  -> coordinator submitted to the pool
	//   - gate full             -> coordinator queued; it runs when a slot frees
	//   - TrySubmit fails (pool absent / shut down) -> step.Run(SerialExecutor) runs synchronously on
	//     the calling thread, a Warning is logged, and the token reads complete.
	//   - token table full (more than kMaxInFlight in flight; scheduler caps this, so it is a wiring
	//     bug): DIA_ASSERT in Debug, the step runs synchronously and an inert, always-complete token
	//     is returned (Release on it is a no-op).
	// A step whose Run throws is treated as complete; the exception is logged as an Error and is not
	// propagated (Launch must not fail, and a worker thread has nowhere to rethrow to).
	//
	// A queued coordinator whose later submit fails (pump-time failure) is detected in IsComplete /
	// Wait, which then run the step inline on SimPU (degrades to synchronous, never hangs).
	class JobStepLauncher final : public IStepLauncher
	{
	public:
		static const uint32_t kMaxInFlight = DeferredStepScheduler::kMaxChannels;

		JobStepLauncher(Dia::Threading::JobSystem& jobs, const CapConfig& config);
		~JobStepLauncher() override;   // asserts nothing is in flight; never blocks
		JobStepLauncher(const JobStepLauncher&) = delete;
		JobStepLauncher& operator=(const JobStepLauncher&) = delete;

		LaunchToken Launch(IDeferredStep& step, const LaunchInfo& info) override;
		bool        IsComplete(LaunchToken token) const override;
		void        Wait(LaunchToken token) override;
		void        Release(LaunchToken token) override;

		CapGate&       GetCapGate()       { return mGate; }
		const CapGate& GetCapGate() const { return mGate; }

	private:
		struct Slot
		{
			bool                                    inUse = false;
			bool                                    viaGate = false;    // item was admitted through the gate
			uint8_t                                 generation = 0;
			IDeferredStep*                          step = nullptr;
			GateItem                                item;
			std::optional<CappedParallelExecutor>   executor;
			std::atomic<bool>                       complete{ false };
		};

		static LaunchToken MakeToken(uint32_t index, uint8_t generation);
		// Returns the slot for a live token, nullptr (after a Debug assert) for zero / stale / released,
		// or nullptr with isOverflow set for the inert overflow token.
		Slot* Resolve(LaunchToken token, bool& isOverflow) const;
		// Runs the slot's step on the calling thread (gate item failed to submit) and marks it complete.
		void  RunInline(Slot& slot) const;
		// Slot finished its gate item as kSubmitFailed but has not run yet: run it inline.
		void  CompleteIfSubmitFailed(Slot& slot) const;
		void  RunStepGuarded(IDeferredStep& step, IParallelExecutor& executor) const;

		CapGate                               mGate;      // declared before mSlots: slots are destroyed first
		mutable std::array<Slot, kMaxInFlight> mSlots;
	};
}
