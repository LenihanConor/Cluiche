#include "TripDetector.h"

namespace Dia
{
	namespace Observation { namespace Profile
	{
		TripDetector::TripDetector() { Reset(); }
		TripDetector::TripDetector(const TripConfig& config) : mConfig(config) { Reset(); }

		void TripDetector::Reset()
		{
			mEntryIndex = 0; mRun = 0; mRunStart = 0; mRunPeak = 0.0f; mUnderStreak = 0;
			mArmed = true; mHasEmitted = false; mLastEmitMs = 0.0; mSuppressedPending = 0;
			mOverTotal = 0; mTripTotal = 0; mSuppressedTotal = 0;
		}

		TripResult TripDetector::Observe(float tickMs, float budgetMs, bool moduleStarting, double nowMs)
		{
			TripResult result;
			const uint32_t idx = mEntryIndex++;

			// AA11: first entry and starting-module entries are skipped entirely.
			if (idx == 0 || moduleStarting)
				return result;

			if (budgetMs <= 0.0f)
			{
				// No budget: counts as under-budget, never trips.
				mRun = 0; mRunPeak = 0.0f;
				++mUnderStreak;
				if (!mArmed && mUnderStreak >= mConfig.rearmFrames) mArmed = true;
				return result;
			}

			const float scale = mConfig.budgetScale > 0.0f ? mConfig.budgetScale : 1.0f;
			const float effective = budgetMs * scale;

			if (!(tickMs > effective))
			{
				mRun = 0; mRunPeak = 0.0f;
				++mUnderStreak;
				if (!mArmed && mUnderStreak >= mConfig.rearmFrames) mArmed = true;
				return result;
			}

			++mOverTotal;
			mUnderStreak = 0;
			if (mRun == 0) { mRunStart = idx; mRunPeak = tickMs; }
			else if (tickMs > mRunPeak) mRunPeak = tickMs;
			++mRun;

			if (!mArmed)
				return result;

			if (mRun >= mConfig.tripFrames || tickMs > mConfig.tripFactor * effective)
			{
				mArmed = false;
				if (mHasEmitted && (nowMs - mLastEmitMs) < mConfig.minIntervalMs)
				{
					++mSuppressedPending;
					++mSuppressedTotal;
					return result;
				}
				result.tripped = true;
				result.firstEntryIndex = mRunStart;
				result.lastEntryIndex = idx;
				result.peakMs = mRunPeak;
				result.suppressedSinceLastTrip = mSuppressedPending;
				mSuppressedPending = 0;
				mHasEmitted = true;
				mLastEmitMs = nowMs;
				++mTripTotal;
			}
			return result;
		}
	}}
}
