#include <DiaDeferredStep/DeferredStepScheduler.h>

#include <chrono>

#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/IStepLauncher.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::DeferredStep
{
	DeferredStepScheduler::DeferredStepScheduler(IStepLauncher& launcher)
		: mLauncher(launcher)
	{
	}

	DeferredStepScheduler::~DeferredStepScheduler()
	{
		for (unsigned int i = 0; i < mChannels.Size(); ++i)
		{
			DrainEntry(mChannels[i]);
		}
		DIA_ASSERT(mChannels.Size() == 0, "DeferredStepScheduler destroyed with registered channels; Unregister in DoStop");
	}

	int DeferredStepScheduler::FindIndex(Dia::Core::StringCRC channelId) const
	{
		for (unsigned int i = 0; i < mChannels.Size(); ++i)
		{
			if (mChannels[i].channelId == channelId)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	bool DeferredStepScheduler::Register(Dia::Core::StringCRC channelId, IDeferredStep& step)
	{
		if (FindIndex(channelId) >= 0)
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Register rejected: channel '%s' already registered", channelId.AsChar());
			return false;
		}
		if (mChannels.IsFull())
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Register rejected: channel '%s', all %u channels in use", channelId.AsChar(), kMaxChannels);
			return false;
		}
		mChannels.AddDefault();
		ChannelEntry& entry = mChannels.Back();
		entry = ChannelEntry();
		entry.channelId = channelId;
		entry.step = &step;
		return true;
	}

	void DeferredStepScheduler::Unregister(Dia::Core::StringCRC channelId)
	{
		const int index = FindIndex(channelId);
		if (index < 0)
		{
			return;
		}
		DrainEntry(mChannels[index]);
		mChannels.RemoveAt(static_cast<unsigned int>(index));
	}

	void DeferredStepScheduler::DrainEntry(ChannelEntry& entry)
	{
		if (!entry.hasToken)
		{
			return;
		}
		const bool wasComplete = mLauncher.IsComplete(entry.token);
		if (wasComplete)
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Channel '%s': discarding completed unapplied result", entry.channelId.AsChar());
		}
		mLauncher.Wait(entry.token);
		mLauncher.Release(entry.token);
		entry.token = LaunchToken();
		entry.hasToken = false;
		entry.state = StepState::kIdle;
		if (mStats.inFlight > 0)
		{
			--mStats.inFlight;
		}
	}

	KickResult DeferredStepScheduler::Kick(Dia::Core::StringCRC channelId, StepKey kickKey, StepKey applyKey)
	{
		const int index = FindIndex(channelId);
		if (index < 0)
		{
			++mStats.kicksRejected;
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Kick rejected: channel '%s' not registered", channelId.AsChar());
			return KickResult::kUnknownChannel;
		}

		ChannelEntry& entry = mChannels[index];
		KickResult rejection = KickResult::kLaunched;
		if (entry.state != StepState::kIdle)
		{
			rejection = KickResult::kAlreadyInFlight;
		}
		else if (applyKey < kickKey)
		{
			rejection = KickResult::kApplyBeforeKick;
		}
		else if (entry.hasKeyFloor && kickKey < entry.keyFloor)
		{
			rejection = KickResult::kKeyNotMonotonic;
		}

		if (rejection != KickResult::kLaunched)
		{
			++mStats.kicksRejected;
			++entry.stats.kicksRejected;
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Kick rejected on channel '%s' (reason %u): kickKey %llu applyKey %llu",
				channelId.AsChar(), static_cast<unsigned int>(rejection), static_cast<unsigned long long>(kickKey), static_cast<unsigned long long>(applyKey));
			return rejection;
		}

		entry.step->Capture();

		entry.kickSequence = ++mKickSequence;
		entry.kickKey = kickKey;
		entry.applyKey = applyKey;
		entry.keyFloor = kickKey;
		entry.hasKeyFloor = true;
		entry.completionSeen = false;
		entry.kickTimeMicros = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
		entry.state = StepState::kRunning;

		LaunchInfo info;
		info.channelId = channelId;
		info.kickKey = kickKey;
		entry.token = mLauncher.Launch(*entry.step, info);
		entry.hasToken = true;

		++entry.stats.kicks;
		++mStats.kicks;
		++mStats.inFlight;
		return KickResult::kLaunched;
	}

	StepState DeferredStepScheduler::GetState(Dia::Core::StringCRC channelId) const
	{
		const int index = FindIndex(channelId);
		return index < 0 ? StepState::kIdle : mChannels[index].state;
	}

	uint32_t DeferredStepScheduler::GetChannelCount() const
	{
		return mChannels.Size();
	}

	const SchedulerStats& DeferredStepScheduler::GetStats() const
	{
		return mStats;
	}

	bool DeferredStepScheduler::GetChannelStats(Dia::Core::StringCRC channelId, ChannelStats& out) const
	{
		const int index = FindIndex(channelId);
		if (index < 0)
		{
			return false;
		}
		out = mChannels[index].stats;
		return true;
	}
}
