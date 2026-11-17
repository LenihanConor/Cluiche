#pragma once

#include <DiaDeferredStep/DeferredStepTypes.h>

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
	};
}
