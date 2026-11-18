#include <DiaDeferredStep/DeferredStepSaveParticipant.h>

#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>

namespace Dia::DeferredStep
{
	const Dia::Core::StringCRC DeferredStepSaveParticipant::kParticipantId("DeferredStep");

	DeferredStepSaveParticipant::DeferredStepSaveParticipant(DeferredStepScheduler& scheduler)
		: mScheduler(scheduler)
	{
	}

	DeferredStepSaveParticipant::~DeferredStepSaveParticipant()
	{
		if (mRegistry != nullptr)
		{
			Detach();
		}
	}

	void DeferredStepSaveParticipant::Attach(Dia::SaveGame::SaveRegistry& registry, Dia::SaveGame::SaveManager& manager)
	{
		DIA_ASSERT(mRegistry == nullptr, "DeferredStepSaveParticipant::Attach called twice");
		if (mRegistry != nullptr)
		{
			return;
		}
		mRegistry = &registry;
		mManager = &manager;
		mRegistry->Register(kParticipantId, this);
		mManager->OnSaveStarted().AttachToObserver(this);
		mManager->OnLoadStarted().AttachToObserver(this);
	}

	void DeferredStepSaveParticipant::Detach()
	{
		DIA_ASSERT(mRegistry != nullptr, "DeferredStepSaveParticipant::Detach without Attach");
		if (mRegistry == nullptr)
		{
			return;
		}
		mManager->OnSaveStarted().DetachFromObserver(this);
		mManager->OnLoadStarted().DetachFromObserver(this);
		mRegistry->Unregister(kParticipantId);
		mRegistry = nullptr;
		mManager = nullptr;
	}

	void DeferredStepSaveParticipant::Serialize(Dia::SaveGame::SaveContext&) const
	{
	}

	void DeferredStepSaveParticipant::Deserialize(Dia::SaveGame::LoadContext&)
	{
	}

	uint32_t DeferredStepSaveParticipant::GetVersion() const
	{
		return kFormatVersion;
	}

	bool DeferredStepSaveParticipant::Validate(Dia::SaveGame::LoadContext&) const
	{
		return true;
	}

	void DeferredStepSaveParticipant::ObserverNotification(const Dia::Core::ObserverSubject*, int message)
	{
		switch (static_cast<Dia::SaveGame::SaveEvent>(message))
		{
		case Dia::SaveGame::SaveEvent::SaveStarted:
		{
			DIA_TRACE_ZONE("DeferredStep.SaveDrain", Dia::Observation::Trace::Category::kNone);
			const uint64_t drainedBefore = mScheduler.GetStats().drainedSteps;
			mScheduler.DrainAll();
			DIA_LOG_INFO(DeferredStepObservation::kLogChannel, "Save drain: %u steps drained, %u pending",
				static_cast<uint32_t>(mScheduler.GetStats().drainedSteps - drainedBefore), mScheduler.GetPendingCount());
			break;
		}
		case Dia::SaveGame::SaveEvent::LoadStarted:
			mScheduler.DrainAndDiscardAll();
			break;
		default:
			break;
		}
	}
}
