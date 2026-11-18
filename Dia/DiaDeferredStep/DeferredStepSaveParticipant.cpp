#include <DiaDeferredStep/DeferredStepSaveParticipant.h>

#include <stdio.h>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>

namespace Dia::DeferredStep
{
	const Dia::Core::StringCRC DeferredStepSaveParticipant::kParticipantId("DeferredStep");

	namespace
	{
		// The file stores only the CRC value; comparison is by value, so rebuild an id from it.
		Dia::Core::StringCRC ChannelIdFromValue(int64_t value)
		{
			Dia::Core::StringCRC channelId;
			static_cast<Dia::Core::CRC&>(channelId) = static_cast<unsigned int>(value);
			return channelId;
		}
	}

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

	void DeferredStepSaveParticipant::Serialize(Dia::SaveGame::SaveContext& ctx) const
	{
		const uint32_t count = mScheduler.GetPendingCount();
		ctx.Write(Dia::Core::StringCRC("version"), static_cast<int32_t>(kFormatVersion));
		ctx.Write(Dia::Core::StringCRC("pending_count"), static_cast<int32_t>(count));
		for (uint32_t i = 0; i < count; ++i)
		{
			PendingStepInfo info;
			if (!mScheduler.GetPendingAt(i, info) || info.step == nullptr)
			{
				continue;
			}
			char name[16];
			snprintf(name, sizeof(name), "channel_%u", i);
			ctx.BeginObject(Dia::Core::StringCRC(name));
			ctx.Write(Dia::Core::StringCRC("channel_id"), static_cast<int64_t>(info.channelId.Value()));
			ctx.Write(Dia::Core::StringCRC("kick_key"), static_cast<int64_t>(info.kickKey));
			ctx.Write(Dia::Core::StringCRC("apply_key"), static_cast<int64_t>(info.applyKey));
			ctx.Write(Dia::Core::StringCRC("step_version"), static_cast<int32_t>(info.step->GetPendingVersion()));
			info.step->SerializePending(ctx);
			ctx.EndObject();
		}
	}

	void DeferredStepSaveParticipant::Deserialize(Dia::SaveGame::LoadContext& ctx)
	{
		DIA_TRACE_ZONE("DeferredStep.Restore", Dia::Observation::Trace::Category::kNone);
		int32_t pendingCount = 0;
		if (!ctx.Read(Dia::Core::StringCRC("pending_count"), pendingCount) || pendingCount < 0)
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "DeferredStep load: save section has no valid 'pending_count'; nothing restored");
			return;
		}
		uint32_t restored = 0;
		for (int32_t i = 0; i < pendingCount; ++i)
		{
			char name[16];
			snprintf(name, sizeof(name), "channel_%d", i);
			if (!ctx.BeginObject(Dia::Core::StringCRC(name)))
			{
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "DeferredStep load: save section is missing '%s'; skipped", name);
				continue;
			}
			int64_t channelValue = 0;
			int64_t kickKey = 0;
			int64_t applyKey = 0;
			if (!ctx.Read(Dia::Core::StringCRC("channel_id"), channelValue)
				|| !ctx.Read(Dia::Core::StringCRC("kick_key"), kickKey)
				|| !ctx.Read(Dia::Core::StringCRC("apply_key"), applyKey))
			{
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "DeferredStep load: '%s' is missing channel_id, kick_key or apply_key; skipped", name);
			}
			else
			{
				const Dia::Core::StringCRC channelId = ChannelIdFromValue(channelValue);
				IDeferredStep* step = mScheduler.GetStep(channelId);
				if (step == nullptr)
				{
					DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "DeferredStep load: saved channel %u is not registered (%s); skipped",
						static_cast<uint32_t>(channelValue), name);
				}
				else
				{
					step->DeserializePending(ctx);
					if (mScheduler.RestorePending(channelId, static_cast<StepKey>(kickKey), static_cast<StepKey>(applyKey)))
					{
						++restored;
					}
					else
					{
						DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "DeferredStep load: channel %u could not be restored (%s); skipped",
							static_cast<uint32_t>(channelValue), name);
					}
				}
			}
			ctx.EndObject();
		}
		DIA_LOG_INFO(DeferredStepObservation::kLogChannel, "Load restore: %u pending steps restored", restored);
	}

	uint32_t DeferredStepSaveParticipant::GetVersion() const
	{
		return kFormatVersion;
	}

	bool DeferredStepSaveParticipant::Validate(Dia::SaveGame::LoadContext& ctx) const
	{
		// Read-only: nothing here touches the scheduler's state or any step's pending data.
		int32_t version = 0;
		if (!ctx.Read(Dia::Core::StringCRC("version"), version))
		{
			DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "DeferredStep load: save section has no 'version'");
			return false;
		}
		if (version < 0 || static_cast<uint32_t>(version) > kFormatVersion)
		{
			DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "DeferredStep load: save format version %d is newer than supported version %u",
				version, kFormatVersion);
			return false;
		}
		int32_t pendingCount = 0;
		if (!ctx.Read(Dia::Core::StringCRC("pending_count"), pendingCount) || pendingCount < 0)
		{
			DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "DeferredStep load: save section has no valid 'pending_count'");
			return false;
		}
		for (int32_t i = 0; i < pendingCount; ++i)
		{
			char name[16];
			snprintf(name, sizeof(name), "channel_%d", i);
			if (!ctx.BeginObject(Dia::Core::StringCRC(name)))
			{
				DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "DeferredStep load: save section is missing '%s'", name);
				return false;
			}
			bool ok = true;
			int64_t channelValue = 0;
			if (!ctx.Read(Dia::Core::StringCRC("channel_id"), channelValue))
			{
				DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "DeferredStep load: '%s' has no 'channel_id'", name);
				ok = false;
			}
			else
			{
				// The file stores only the CRC value; comparison is by value, so rebuild an id from it.
				Dia::Core::StringCRC channelId;
				static_cast<Dia::Core::CRC&>(channelId) = static_cast<unsigned int>(channelValue);
				IDeferredStep* step = mScheduler.GetStep(channelId);
				if (step == nullptr)
				{
					DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "DeferredStep load: saved channel %u is not registered (%s)",
						static_cast<uint32_t>(channelValue), name);
					ok = false;
				}
				else if (!step->ValidatePending(ctx))
				{
					DIA_LOG_ERROR(DeferredStepObservation::kLogChannel, "DeferredStep load: step on channel %u rejected its pending data",
						static_cast<uint32_t>(channelValue));
					ok = false;
				}
			}
			ctx.EndObject();
			if (!ok)
			{
				return false;
			}
		}
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
