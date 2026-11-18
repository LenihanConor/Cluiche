#pragma once

#include <stdint.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaThreading/JobContext.h>

namespace Dia::DeferredStep
{
	using StepKey = uint64_t;

	// Opaque handle to one launched step. 0 is invalid.
	struct LaunchToken
	{
		uint32_t value = 0;
		bool IsValid() const { return value != 0; }
	};

	// Attribution passed to the launcher; Feature 5 turns this into a JobTag.
	struct LaunchInfo
	{
		Dia::Core::StringCRC channelId;
		StepKey kickKey = 0;
	};

	// JobTag.range value that marks a step's coordinator job (rangeCount is 0 for it).
	constexpr uint16_t kCoordinatorRange = 0xFFFF;

	// Attribution for one job of a step: owner = channel id, tag = kick key, range/rangeCount as given.
	inline Dia::Threading::JobTag MakeJobTag(const LaunchInfo& info, uint16_t range, uint16_t rangeCount)
	{
		Dia::Threading::JobTag t;
		t.owner = info.channelId.Value();
		// LOSSY: JobTag.tag is 32 bits, so only the low 32 bits of the 64-bit kick key survive.
		t.tag = static_cast<uint32_t>(info.kickKey & 0xFFFFFFFFull);
		t.range = range;
		t.rangeCount = rangeCount;
		return t;
	}

	// Raw timestamps of one applied step (steady_clock nanoseconds since epoch, the clock RangeTiming uses).
	// kickNs/readyNs are 0 for a restored step (no real launch). readyNs is when completion was first observed.
	struct StepTimes
	{
		uint64_t kickNs  = 0;
		uint64_t readyNs = 0;
		uint64_t applyNs = 0;
		uint64_t stallNs = 0;
	};

	enum class StepState : uint8_t
	{
		kIdle,
		kRunning,
		kComplete
	};

	enum class KickResult : uint8_t
	{
		kLaunched,
		kUnknownChannel,   // no step registered on that channel
		kAlreadyInFlight,  // one step per channel
		kKeyNotMonotonic,  // kickKey < this channel's previous kickKey
		kApplyBeforeKick   // applyKey < kickKey
	};
}
