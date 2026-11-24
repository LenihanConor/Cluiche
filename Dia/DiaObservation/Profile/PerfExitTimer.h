#pragma once

#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Perf Inspector A4c: pure wall-clock exit timer for `--perf=<seconds>`.
		// No clock of its own: the caller injects "now" (nanoseconds, any monotonic origin).
		// ShouldExit() latches true once the deadline passes so the quit request fires once.
		class PerfExitTimer
		{
		public:
			void Arm(uint32_t seconds, int64_t nowNs)
			{
				mArmed = seconds > 0;
				mFired = false;
				mDeadlineNs = nowNs + static_cast<int64_t>(seconds) * 1000000000LL;
			}

			// True exactly once, on the first call at or after the deadline.
			bool ShouldExit(int64_t nowNs)
			{
				if (!mArmed || mFired || nowNs < mDeadlineNs)
					return false;
				mFired = true;
				return true;
			}

			bool IsArmed() const { return mArmed; }

		private:
			bool    mArmed = false;
			bool    mFired = false;
			int64_t mDeadlineNs = 0;
		};
	} }
}
