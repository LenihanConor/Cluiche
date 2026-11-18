#pragma once

#include <stdint.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaDeferredStep/DeferredStepTypes.h>

namespace Dia::DeferredStep
{
	class IDeferredStep;
	class IStepLauncher;

	struct ChannelStats
	{
		uint64_t kicks = 0;
		uint64_t kicksRejected = 0;
		uint64_t applies = 0;
		uint64_t stalls = 0;
		uint64_t lastRunMicros = 0;     // Kick -> completion first observed, on SimPU's clock
		uint64_t lastStallMicros = 0;
		uint64_t totalStallMicros = 0;
		int64_t  lastSlackKeys = 0;     // applyKey - key at which completion was first seen
	};

	struct SchedulerStats
	{
		uint32_t inFlight = 0;
		uint64_t kicks = 0;
		uint64_t kicksRejected = 0;
		uint64_t applies = 0;
		uint64_t stalls = 0;
		uint64_t totalStallMicros = 0;
		uint64_t drains = 0;
		uint64_t drainedSteps = 0;
		int64_t  lastApplySlackKeys = 0;   // slack of the most recently applied step (0 if none yet)
	};

	// A completed-but-unapplied step, as reported by GetPendingAt.
	struct PendingStepInfo
	{
		Dia::Core::StringCRC channelId;
		StepKey              kickKey  = 0;
		StepKey              applyKey = 0;
		IDeferredStep*       step     = nullptr;
	};

	// Lifecycle owner for deferred steps: one in-flight step per registered channel. SimPU only.
	class DeferredStepScheduler
	{
	public:
		static const unsigned int kMaxChannels = 8;

		explicit DeferredStepScheduler(IStepLauncher& launcher);
		~DeferredStepScheduler();   // drains in-flight steps, then asserts no channel is registered

		DeferredStepScheduler(const DeferredStepScheduler&) = delete;
		DeferredStepScheduler& operator=(const DeferredStepScheduler&) = delete;

		bool Register  (Dia::Core::StringCRC channelId, IDeferredStep& step);
		void Unregister(Dia::Core::StringCRC channelId);   // drains that channel, discards unapplied

		// Capture() then Launch() on the calling (SimPU) thread; one step per channel in flight.
		// Checks in order: unknown channel, already in flight, applyKey < kickKey, kickKey < previous kickKey.
		// A rejected kick does nothing but log a Warning and bump kicksRejected.
		// applyKey == kickKey is accepted: it is synchronous and will stall at the next Tick.
		KickResult Kick(Dia::Core::StringCRC channelId, StepKey kickKey, StepKey applyKey);

		// SimPU, once per drained sim step. Applies every step whose applyKey <= currentKey in kick order.
		// A currentKey lower than the previous call asserts (Debug), logs a Warning and applies nothing.
		void Tick(StepKey currentKey);

		// Waits for every in-flight step but does NOT apply: each channel moves to kComplete with its applyKey intact
		// and a later Tick applies it normally. No-op (no Wait, no log, drains unchanged) when nothing is in flight.
		void DrainAll();

		// Completed-but-unapplied steps (kComplete), in kick order. Valid after DrainAll.
		uint32_t GetPendingCount() const;
		bool     GetPendingAt(uint32_t index, PendingStepInfo& out) const;

		// Waits for every in-flight step and discards it without applying; restored (token-less) complete channels are
		// discarded too. All channels end kIdle; key floors are kept. Load, and shutdown.
		void DrainAndDiscardAll();

		// Puts a registered idle channel into kComplete with no launch token, so the next Tick at or past applyKey applies it
		// without consulting the launcher. The channel's monotonic key floor becomes kickKey. False (Warning) otherwise.
		bool RestorePending(Dia::Core::StringCRC channelId, StepKey kickKey, StepKey applyKey);

		// True with the channel's applyKey while kRunning or kComplete; false for unknown or idle channels.
		bool GetApplyKey(Dia::Core::StringCRC channelId, StepKey& out) const;

		StepState GetState       (Dia::Core::StringCRC channelId) const;
		uint32_t  GetChannelCount() const;
		const SchedulerStats& GetStats() const;
		bool      GetChannelStats(Dia::Core::StringCRC channelId, ChannelStats& out) const;

		// Timestamps of the most recently applied step; false until one has applied. Discarded steps never update it.
		bool      GetLastStepTimes(StepTimes& out) const;

		// The step registered on a channel, or nullptr when the channel is not registered.
		IDeferredStep* GetStep(Dia::Core::StringCRC channelId) const;

	private:
		struct ChannelEntry
		{
			Dia::Core::StringCRC channelId;
			IDeferredStep* step = nullptr;
			StepState state = StepState::kIdle;
			StepKey kickKey = 0;
			StepKey applyKey = 0;
			StepKey keyFloor = 0;          // previous kickKey; persists after apply
			bool hasKeyFloor = false;      // false until the first accepted kick
			LaunchToken token;
			bool hasToken = false;
			uint64_t kickSequence = 0;
			StepKey completionSeenKey = 0;
			bool completionSeen = false;
			uint64_t kickTimeMicros = 0;
			uint64_t kickNs = 0;
			uint64_t readyNs = 0;          // when completion was first observed (0 until then)
			ChannelStats stats;
		};

		int  FindIndex(Dia::Core::StringCRC channelId) const;
		void ApplyRestored(ChannelEntry& entry, Dia::Core::StringCRC channelId);   // Tick: apply a token-less kComplete channel
		void DrainEntry(ChannelEntry& entry);   // Wait if in flight, Release, no Apply; also resets a restored token-less kComplete channel

		IStepLauncher& mLauncher;
		Dia::Core::Containers::DynamicArrayC<ChannelEntry, kMaxChannels> mChannels;
		SchedulerStats mStats;
		uint64_t mKickSequence = 0;
		StepTimes mLastTimes;
		bool mHasLastTimes = false;
		StepKey mLastTickKey = 0;
		bool mHasTicked = false;
		bool mInBackwardsEpisode = false;   // latched on the first backwards Tick; cleared when the key reaches mLastTickKey again
	};
}
