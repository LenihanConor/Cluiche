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

		StepState GetState       (Dia::Core::StringCRC channelId) const;
		uint32_t  GetChannelCount() const;
		const SchedulerStats& GetStats() const;
		bool      GetChannelStats(Dia::Core::StringCRC channelId, ChannelStats& out) const;

	private:
		struct ChannelEntry
		{
			Dia::Core::StringCRC channelId;
			IDeferredStep* step = nullptr;
			StepState state = StepState::kIdle;
			StepKey kickKey = 0;
			StepKey applyKey = 0;
			StepKey keyFloor = 0;
			LaunchToken token;
			bool hasToken = false;
			uint64_t kickSequence = 0;
			StepKey completionSeenKey = 0;
			bool completionSeen = false;
			uint64_t kickTimeMicros = 0;
			ChannelStats stats;
		};

		int  FindIndex(Dia::Core::StringCRC channelId) const;
		void DrainEntry(ChannelEntry& entry);   // Wait if in flight, Release, no Apply

		IStepLauncher& mLauncher;
		Dia::Core::Containers::DynamicArrayC<ChannelEntry, kMaxChannels> mChannels;
		SchedulerStats mStats;
	};
}
