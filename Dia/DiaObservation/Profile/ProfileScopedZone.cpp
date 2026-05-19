#include "DiaObservation/Profile/ScopedZone.h"
#include "DiaObservation/Profile/Profiler.h"
#include "DiaObservation/Metric/Histogram.h"

#include <chrono>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Dia
{
	namespace Observation { namespace Profile
	{
		ScopedZone::ScopedZone(const Dia::Core::StringCRC& name, ProfileCategory category,
		                       Dia::Observation::Metric::Histogram* histogram)
			: mScopeId(0)
			, mParentScopeId(0)
			, mStartNs(0)
			, mFrameNumber(0)
			, mHistogram(histogram)
			, mActive(false)
		{
			// Early-out if this category is not active
			if (!(Profiler::Instance().ActiveMask() & category))
				return;

			mStartNs = static_cast<uint64_t>(
				std::chrono::steady_clock::now().time_since_epoch().count());

			mScopeId = Profiler::Instance().OnScopeOpen(name, category, mFrameNumber, mParentScopeId);
			if (mScopeId == 0)
				return; // Overflow or not started

			mActive   = true;
			mName     = name;
			mCategory = category;
		}

		ScopedZone::~ScopedZone()
		{
			uint64_t endNs      = 0;
			uint64_t durationNs = 0;

			if (mHistogram != nullptr || mActive)
			{
				endNs      = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
				durationNs = (endNs >= mStartNs) ? (endNs - mStartNs) : 0;
			}

			if (mHistogram != nullptr)
			{
				// Always record to histogram regardless of mActive
				mHistogram->Observe(static_cast<double>(durationNs));
			}

			if (!mActive)
				return;

			uint32_t threadId = 0;
#ifdef _WIN32
			threadId = static_cast<uint32_t>(GetCurrentThreadId());
#endif

			Profiler::Instance().OnScopeClose(
				mScopeId, mName, mCategory, mParentScopeId,
				mStartNs, durationNs, mFrameNumber, threadId);
		}
	}
} // namespace Observation
} // namespace Dia
