#pragma once

#include <stdint.h>
#include <DiaCore/CRC/StringCRC.h>

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
