#include <DiaDeferredStep/JobStepLauncher.h>

#include <exception>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/SerialExecutor.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::DeferredStep
{
	namespace
	{
		// Token layout: bits 0-7 = slot index + 1 (never 0), bits 8-15 = generation.
		// Index kMaxInFlight (value kMaxInFlight + 1) is the reserved inert overflow token.
		const uint32_t kIndexMask = 0xFFu;
		const uint32_t kGenerationShift = 8;
	}

	LaunchToken JobStepLauncher::MakeToken(uint32_t index, uint8_t generation)
	{
		LaunchToken t;
		t.value = (index + 1u) | (static_cast<uint32_t>(generation) << kGenerationShift);
		return t;
	}

	JobStepLauncher::JobStepLauncher(Dia::Threading::JobSystem& jobs, const CapConfig& config)
		: mGate(jobs, config)
	{
	}

	JobStepLauncher::~JobStepLauncher()
	{
		// Never blocks: the scheduler drains (Wait + Release every token) before the launcher dies.
		for (const Slot& slot : mSlots)
		{
			DIA_ASSERT(!slot.inUse, "JobStepLauncher destroyed with a launched step still in flight");
		}
	}

	void JobStepLauncher::RunStepGuarded(IDeferredStep& step, IParallelExecutor& executor) const
	{
		try
		{
			step.Run(executor);
		}
		catch (const std::exception& e)
		{
			DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "Deferred step Run threw: %s; treating it as complete", e.what());
		}
		catch (...)
		{
			DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "Deferred step Run threw an unknown exception; treating it as complete");
		}
	}

	JobStepLauncher::Slot* JobStepLauncher::Resolve(LaunchToken token, bool& isOverflow) const
	{
		isOverflow = false;
		if (!token.IsValid())
		{
			DIA_ASSERT(false, "JobStepLauncher: zero token");
			return nullptr;
		}
		const uint32_t index = (token.value & kIndexMask) - 1u;   // wraps to a huge value for index byte 0
		const uint8_t generation = static_cast<uint8_t>((token.value >> kGenerationShift) & 0xFFu);
		if (index == kMaxInFlight)
		{
			isOverflow = true;
			return nullptr;
		}
		if (index >= kMaxInFlight)
		{
			DIA_ASSERT(false, "JobStepLauncher: token index out of range");
			return nullptr;
		}
		Slot& slot = mSlots[index];
		if (!slot.inUse || slot.generation != generation)
		{
			DIA_ASSERT(false, "JobStepLauncher: stale or released token");
			return nullptr;
		}
		return &slot;
	}

	void JobStepLauncher::RunInline(Slot& slot) const
	{
		SerialExecutor serial;
		RunStepGuarded(*slot.step, serial);
		slot.complete.store(true);
	}

	void JobStepLauncher::CompleteIfSubmitFailed(Slot& slot) const
	{
		if (!slot.complete.load() && slot.item.GetState() == GateItem::State::kSubmitFailed)
		{
			// Queued coordinator whose pump-time submit failed: nobody else will ever run it.
			// Degrades to synchronous on SimPU.
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel,
				"Queued step could not be submitted; running it inline on the calling thread");
			RunInline(slot);
		}
	}

	LaunchToken JobStepLauncher::Launch(IDeferredStep& step, const LaunchInfo& info)
	{
		uint32_t index = kMaxInFlight;
		for (uint32_t i = 0; i < kMaxInFlight; ++i)
		{
			if (!mSlots[i].inUse) { index = i; break; }
		}

		if (index == kMaxInFlight)
		{
			DIA_ASSERT(false, "JobStepLauncher: token table full (%u in flight)", kMaxInFlight);
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel,
				"Launch with %u steps already in flight; running synchronously", kMaxInFlight);
			SerialExecutor serial;
			RunStepGuarded(step, serial);
			return MakeToken(kMaxInFlight, 0);   // inert, always complete
		}

		Slot& slot = mSlots[index];
		slot.inUse = true;
		slot.step = &step;
		slot.complete.store(false);
		slot.executor.emplace(mGate, info);

		// The step lives until the scheduler has Wait()ed and Release()d this token (it drains before
		// Unregister and before destruction), so capturing a raw pointer is safe. The slot is
		// address-stable (member array) and stays in use until Release.
		Slot* slotPtr = &slot;
		slot.item.Prepare(GateItem::Kind::kCoordinator, [this, slotPtr]
		{
			RunStepGuarded(*slotPtr->step, *slotPtr->executor);
			slotPtr->complete.store(true);   // before the gate epilogue; Release waits for the item to finish
		});

		// Coordinator job: same owner/tag as its ranges, range = kCoordinatorRange, rangeCount = 0.
		slot.item.SetJobTag(MakeJobTag(info, kCoordinatorRange, 0));

		const AdmitResult result = mGate.Admit(slot.item);
		slot.viaGate = true;
		if (result == AdmitResult::kSubmitFailed)
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel,
				"Coordinator submit failed (job pool unavailable?); running step synchronously");
			RunInline(slot);
		}
		return MakeToken(index, slot.generation);
	}

	bool JobStepLauncher::IsComplete(LaunchToken token) const
	{
		bool isOverflow = false;
		Slot* slot = Resolve(token, isOverflow);
		if (slot == nullptr)
		{
			return true;   // invalid / stale / overflow: nothing is running under it
		}
		if (slot->complete.load())
		{
			return true;
		}
		CompleteIfSubmitFailed(*slot);
		return slot->complete.load();
	}

	void JobStepLauncher::Wait(LaunchToken token)
	{
		bool isOverflow = false;
		Slot* slot = Resolve(token, isOverflow);
		if (slot == nullptr || slot->complete.load())
		{
			return;   // AC 22: no gate / JobSystem access for an already-complete token
		}
		if (mGate.WaitItem(slot->item) == GateItem::State::kSubmitFailed)
		{
			CompleteIfSubmitFailed(*slot);
		}
	}

	void JobStepLauncher::Release(LaunchToken token)
	{
		bool isOverflow = false;
		Slot* slot = Resolve(token, isOverflow);
		if (slot == nullptr)
		{
			return;   // overflow token or invalid: no-op
		}
		if (!slot->complete.load())
		{
			DIA_ASSERT(false, "JobStepLauncher: Release of a step that is still running");
			Wait(token);
		}
		// complete is set inside the work; the gate epilogue still touches the item afterwards, so
		// wait for the item to be fully finished before the slot can be reused.
		if (slot->viaGate)
		{
			mGate.WaitItem(slot->item);
		}
		slot->executor.reset();
		slot->step = nullptr;
		slot->viaGate = false;
		slot->inUse = false;
		slot->generation = static_cast<uint8_t>(slot->generation + 1u);
	}
}
