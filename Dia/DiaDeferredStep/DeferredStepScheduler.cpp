#include <DiaDeferredStep/DeferredStepScheduler.h>

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
