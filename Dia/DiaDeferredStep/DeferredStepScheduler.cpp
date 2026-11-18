#include <DiaDeferredStep/DeferredStepScheduler.h>

#include <chrono>

#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/IStepLauncher.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

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
			if (entry.state == StepState::kComplete)
			{
				// Restored pending step: no token, no launcher calls.
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Channel '%s': discarding restored unapplied result", entry.channelId.AsChar());
				entry.state = StepState::kIdle;
			}
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

		{
			DIA_TRACE_ZONE("DeferredStep.Capture", Dia::Observation::Trace::Category::kNone);
			DIA_PROFILE_SCOPE("DeferredStep.Capture", Dia::Observation::Profile::Category::kNone);
			entry.step->Capture();
		}

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

		// Poll pass: for in-flight, not-yet-due channels, call IsComplete exactly once per Tick until completion is first
		// seen. Slack and run time are written to ChannelStats at observation (not at apply) so they are readable early.
		if (mStats.inFlight > 0)
		{
			for (unsigned int i = 0; i < mChannels.Size(); ++i)
			{
				ChannelEntry& e = mChannels[i];
				if (e.hasToken && e.state == StepState::kRunning && e.applyKey > currentKey && !e.completionSeen
					&& mLauncher.IsComplete(e.token))
				{
					e.completionSeen = true;
					e.completionSeenKey = currentKey;
					e.stats.lastRunMicros = NowMicros() - e.kickTimeMicros;
					e.stats.lastSlackKeys = static_cast<int64_t>(e.applyKey - currentKey); // currentKey < applyKey, no underflow
				}
			}
		}

		// Collect due entries (no heap), then order them by kick sequence.
		unsigned int due[kMaxChannels];
		unsigned int dueCount = 0;
		for (unsigned int i = 0; i < mChannels.Size(); ++i)
		{
			const ChannelEntry& e = mChannels[i];
			if ((e.state == StepState::kRunning || e.state == StepState::kComplete) && (e.hasToken || e.state == StepState::kComplete)
				&& e.applyKey <= currentKey)
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
			if (entry.state != StepState::kRunning && entry.state != StepState::kComplete)
			{
				continue;
			}
			if (!entry.hasToken)
			{
				ApplyRestored(entry, dueIds[i]);
				continue;
			}
			const LaunchToken token = entry.token;
			IDeferredStep* step = entry.step;
			const StepKey applyKey = entry.applyKey;
			bool stalled = false;
			uint64_t stallMicros = 0;
			const bool alreadySeen = entry.completionSeen;
			uint64_t runMicros = 0;
			if (!mLauncher.IsComplete(token))
			{
				const uint64_t waitStart = NowMicros();
				{
					DIA_TRACE_ZONE("DeferredStep.StallWait", Dia::Observation::Trace::Category::kNone);
					DIA_PROFILE_SCOPE("DeferredStep.StallWait", Dia::Observation::Profile::Category::kNone);
					mLauncher.Wait(token);
				}
				const uint64_t waitEnd = NowMicros();
				stallMicros = waitEnd - waitStart;
				runMicros = waitEnd - entry.kickTimeMicros;   // upper bound: Kick -> after Wait returns
				stalled = true;
			}
			else if (!alreadySeen)
			{
				runMicros = NowMicros() - entry.kickTimeMicros;   // first observed on the apply key itself
			}
			{
				DIA_TRACE_ZONE("DeferredStep.Apply", Dia::Observation::Trace::Category::kNone);
				DIA_PROFILE_SCOPE("DeferredStep.Apply", Dia::Observation::Profile::Category::kNone);
				step->Apply();
			}

			// Apply may have unregistered this channel (Unregister already drained and released its token), so only
			// release and settle the entry if it still holds the token this Tick launched the apply for.
			const int after = FindIndex(dueIds[i]);
			const bool owned = after >= 0 && mChannels[after].hasToken && mChannels[after].token.value == token.value;
			if (owned)
			{
				mLauncher.Release(token);
			}
			if (stalled)
			{
				++mStats.stalls;
				mStats.totalStallMicros += stallMicros;
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Channel '%s' stalled at apply key %llu for %llu us",
					dueIds[i].AsChar(), static_cast<unsigned long long>(applyKey), static_cast<unsigned long long>(stallMicros));
			}
			if (owned)
			{
				ChannelEntry& done = mChannels[after];
				if (!alreadySeen)
				{
					// First seen at apply (or stalled): slack is 0, never negative.
					done.stats.lastRunMicros = runMicros;
					done.stats.lastSlackKeys = 0;
				}
				done.completionSeen = true;
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
			mStats.lastApplySlackKeys = owned ? mChannels[after].stats.lastSlackKeys : 0;
			++mStats.applies;
			if (owned && mStats.inFlight > 0)
			{
				--mStats.inFlight;
			}
		}
	}

	void DeferredStepScheduler::ApplyRestored(ChannelEntry& entry, Dia::Core::StringCRC channelId)
	{
		// A restored channel has no launch token: complete by construction, so no IsComplete/Wait/Release and no stall.
		IDeferredStep* step = entry.step;
		const uint64_t sequence = entry.kickSequence;
		{
			DIA_TRACE_ZONE("DeferredStep.Apply", Dia::Observation::Trace::Category::kNone);
			DIA_PROFILE_SCOPE("DeferredStep.Apply", Dia::Observation::Profile::Category::kNone);
			step->Apply();
		}
		// Apply may have unregistered or reset the channel; only settle it if it is still the same restored step.
		const int after = FindIndex(channelId);
		if (after >= 0)
		{
			ChannelEntry& done = mChannels[after];
			if (done.state == StepState::kComplete && !done.hasToken && done.kickSequence == sequence)
			{
				done.state = StepState::kIdle;
				++done.stats.applies;
			}
		}
		mStats.lastApplySlackKeys = 0;
		++mStats.applies;
	}

	uint32_t DeferredStepScheduler::GetPendingCount() const
	{
		uint32_t count = 0;
		for (unsigned int i = 0; i < mChannels.Size(); ++i)
		{
			if (mChannels[i].state == StepState::kComplete)
			{
				++count;
			}
		}
		return count;
	}

	bool DeferredStepScheduler::GetPendingAt(uint32_t index, PendingStepInfo& out) const
	{
		// Select the index-th complete channel in kick-sequence order (rank by counting smaller sequences).
		for (unsigned int i = 0; i < mChannels.Size(); ++i)
		{
			const ChannelEntry& e = mChannels[i];
			if (e.state != StepState::kComplete)
			{
				continue;
			}
			uint32_t rank = 0;
			for (unsigned int j = 0; j < mChannels.Size(); ++j)
			{
				if (mChannels[j].state == StepState::kComplete && mChannels[j].kickSequence < e.kickSequence)
				{
					++rank;
				}
			}
			if (rank == index)
			{
				out.channelId = e.channelId;
				out.kickKey = e.kickKey;
				out.applyKey = e.applyKey;
				out.step = e.step;
				return true;
			}
		}
		return false;
	}

	void DeferredStepScheduler::DrainAndDiscardAll()
	{
		DIA_TRACE_ZONE("DeferredStep.DrainAndDiscardAll", Dia::Observation::Trace::Category::kNone);
		DIA_PROFILE_SCOPE("DeferredStep.DrainAndDiscardAll", Dia::Observation::Profile::Category::kNone);
		uint32_t discarded = 0;
		uint32_t inFlightDiscarded = 0;
		for (unsigned int i = 0; i < mChannels.Size(); ++i)
		{
			ChannelEntry& e = mChannels[i];
			if (e.state == StepState::kIdle && !e.hasToken)
			{
				continue;
			}
			if (e.state == StepState::kRunning)
			{
				++inFlightDiscarded;
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "Channel '%s': discarding in-flight step", e.channelId.AsChar());
			}
			DrainEntry(e);
			++discarded;
		}
		if (discarded == 0)
		{
			return;
		}
		DIA_LOG_INFO(DeferredStepObservation::kLogChannel, "DrainAndDiscardAll discarded %u step(s), %u were still in flight", discarded, inFlightDiscarded);
	}

	bool DeferredStepScheduler::RestorePending(Dia::Core::StringCRC channelId, StepKey kickKey, StepKey applyKey)
	{
		const int index = FindIndex(channelId);
		if (index < 0)
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "RestorePending rejected: channel '%s' not registered", channelId.AsChar());
			return false;
		}
		ChannelEntry& e = mChannels[index];
		if (e.state != StepState::kIdle)
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "RestorePending rejected: channel '%s' is not idle", channelId.AsChar());
			return false;
		}
		e.state = StepState::kComplete;
		e.token = LaunchToken();
		e.hasToken = false;
		e.kickKey = kickKey;
		e.applyKey = applyKey;
		e.keyFloor = kickKey;
		e.hasKeyFloor = true;
		e.kickSequence = ++mKickSequence;
		e.completionSeen = true;
		return true;
	}

	void DeferredStepScheduler::DrainAll()
	{
		DIA_TRACE_ZONE("DeferredStep.DrainAll", Dia::Observation::Trace::Category::kNone);
		DIA_PROFILE_SCOPE("DeferredStep.DrainAll", Dia::Observation::Profile::Category::kNone);
		uint32_t drained = 0;
		for (unsigned int i = 0; i < mChannels.Size(); ++i)
		{
			ChannelEntry& e = mChannels[i];
			if (!e.hasToken || e.state != StepState::kRunning)
			{
				continue;
			}
			mLauncher.Wait(e.token);
			e.state = StepState::kComplete;   // token, hasToken and applyKey kept: Tick still applies and releases it
			if (!e.completionSeen)
			{
				// A drained step has no meaningful slack; run time is kick -> after Wait.
				e.completionSeen = true;
				e.stats.lastRunMicros = NowMicros() - e.kickTimeMicros;
				e.stats.lastSlackKeys = 0;
			}
			++drained;
		}
		if (drained == 0)
		{
			return;
		}
		++mStats.drains;
		mStats.drainedSteps += drained;
		DIA_LOG_INFO(DeferredStepObservation::kLogChannel, "DrainAll waited for %u in-flight step(s)", drained);
	}

	bool DeferredStepScheduler::GetApplyKey(Dia::Core::StringCRC channelId, StepKey& out) const
	{
		const int index = FindIndex(channelId);
		if (index < 0)
		{
			return false;
		}
		const ChannelEntry& e = mChannels[index];
		if (e.state != StepState::kRunning && e.state != StepState::kComplete)
		{
			return false;
		}
		out = e.applyKey;
		return true;
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

	IDeferredStep* DeferredStepScheduler::GetStep(Dia::Core::StringCRC channelId) const
	{
		const int index = FindIndex(channelId);
		return index < 0 ? nullptr : mChannels[index].step;
	}
}
