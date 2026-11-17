#include <DiaDeferredStep/DeferredStepScheduler.h>

#include <chrono>

#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/IStepLauncher.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::DeferredStep
{
	namespace
	{
		uint64_t NowMicros()
		{
			return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count());
		}
	}

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
		entry.kickTimeMicros = NowMicros();
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

	void DeferredStepScheduler::Tick(StepKey currentKey)
	{
		if (mHasTicked && currentKey < mLastTickKey)
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Tick key went backwards: %llu < previous %llu; nothing applied",
				static_cast<unsigned long long>(currentKey), static_cast<unsigned long long>(mLastTickKey));
			DIA_ASSERT(false, "DeferredStepScheduler::Tick called with a key lower than the previous call");
			return;
		}
		mLastTickKey = currentKey;
		mHasTicked = true;

		// Collect due entries (no heap), then order them by kick sequence.
		unsigned int due[kMaxChannels];
		unsigned int dueCount = 0;
		for (unsigned int i = 0; i < mChannels.Size(); ++i)
		{
			const ChannelEntry& e = mChannels[i];
			if (e.hasToken && e.state == StepState::kRunning && e.applyKey <= currentKey)
			{
				unsigned int pos = dueCount++;
				while (pos > 0 && mChannels[due[pos - 1]].kickSequence > e.kickSequence)
				{
					due[pos] = due[pos - 1];
					--pos;
				}
				due[pos] = i;
			}
		}
		if (dueCount == 0)
		{
			return;
		}

		// Apply may re-enter the scheduler, so re-resolve each channel by id rather than holding indices.
		Dia::Core::StringCRC dueIds[kMaxChannels];
		for (unsigned int i = 0; i < dueCount; ++i)
		{
			dueIds[i] = mChannels[due[i]].channelId;
		}
		for (unsigned int i = 0; i < dueCount; ++i)
		{
			const int index = FindIndex(dueIds[i]);
			if (index < 0)
			{
				continue;
			}
			ChannelEntry& entry = mChannels[index];
			if (!entry.hasToken || entry.state != StepState::kRunning)
			{
				continue;
			}
			const LaunchToken token = entry.token;
			IDeferredStep* step = entry.step;
			const StepKey applyKey = entry.applyKey;
			bool stalled = false;
			uint64_t stallMicros = 0;
			if (!mLauncher.IsComplete(token))
			{
				const uint64_t waitStart = NowMicros();
				mLauncher.Wait(token);
				stallMicros = NowMicros() - waitStart;
				stalled = true;
			}
			step->Apply();
			mLauncher.Release(token);

			const int after = FindIndex(dueIds[i]);
			if (stalled)
			{
				++mStats.stalls;
				mStats.totalStallMicros += stallMicros;
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Channel '%s' stalled at apply key %llu for %llu us",
					dueIds[i].AsChar(), static_cast<unsigned long long>(applyKey), static_cast<unsigned long long>(stallMicros));
			}
			if (after >= 0)
			{
				ChannelEntry& done = mChannels[after];
				if (stalled)
				{
					++done.stats.stalls;
					done.stats.lastStallMicros = stallMicros;
					done.stats.totalStallMicros += stallMicros;
				}
				done.token = LaunchToken();
				done.hasToken = false;
				done.state = StepState::kIdle;
				++done.stats.applies;
			}
			++mStats.applies;
			if (mStats.inFlight > 0)
			{
				--mStats.inFlight;
			}
		}
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
