#pragma once

#include <stdint.h>
#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/RangeTiming.h>

namespace Dia::DeferredStep
{
	class IDeferredStep;

	class IStepLauncher
	{
	public:
		virtual ~IStepLauncher() = default;

		// Must always return a valid token. A launcher that cannot go async runs step.Run(...) synchronously
		// and returns an already-complete token.
		virtual LaunchToken Launch(IDeferredStep& step, const LaunchInfo& info) = 0;

		virtual bool IsComplete(LaunchToken token) const = 0;  // cheap, non-blocking
		virtual void Wait(LaunchToken token) = 0;              // blocks until Run has returned
		virtual void Release(LaunchToken token) = 0;           // scheduler finished with token

		// Observation seams (non-pure so existing launchers compile unchanged). Called by the scheduler on SimPU for a
		// completed token BEFORE Release. The returned pointer is valid until Release.
		virtual uint32_t GetRangeTimings(LaunchToken token, const RangeTiming*& outTimings) const
		{
			(void)token;
			outTimings = nullptr;
			return 0;
		}
		virtual bool RanInlineWholly(LaunchToken token) const { (void)token; return false; }
	};
}
