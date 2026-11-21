#pragma once

#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Pure per-unit over-budget trip detector. No clock inside: callers pass nowMs.
		// One Observe() call = one entry (one forward pass; on Sim, one fixed step).
		//
		// Skipped entries (neither over nor under; do not count toward, trip, or break a run):
		//   - the unit's FIRST entry, and any entry with moduleStarting == true.
		// An entry with budgetMs <= 0 (no budget) never trips and counts as under-budget.
		// budgetScale <= 0 is treated as 1.0. Boundary: tickMs == effective budget is NOT over.
		struct TripConfig
		{
			uint32_t tripFrames    = 10;      // consecutive over-budget entries to trip
			float    tripFactor    = 2.0f;    // single entry > factor * budget trips
			uint32_t rearmFrames   = 30;      // consecutive under-budget entries to re-arm
			double   minIntervalMs = 5000.0;  // min gap between EMITTED trips
			float    budgetScale   = 1.0f;    // multiplies budget; <= 0 means 1.0
		};

		struct TripResult
		{
			bool     tripped                 = false;
			uint32_t firstEntryIndex         = 0;   // first over-budget entry of the current run
			uint32_t lastEntryIndex          = 0;   // current entry index
			float    peakMs                  = 0.0f; // peak tickMs within the run
			uint32_t suppressedSinceLastTrip = 0;
		};

		class TripDetector
		{
		public:
			TripDetector();
			explicit TripDetector(const TripConfig& config);

			void Reset();
			void SetConfig(const TripConfig& config) { mConfig = config; }
			const TripConfig& GetConfig() const { return mConfig; }

			TripResult Observe(float tickMs, float budgetMs, bool moduleStarting, double nowMs);

			bool     IsArmed() const { return mArmed; }
			uint32_t GetOverBudgetFrames() const { return mOverTotal; }
			uint32_t GetTripCount() const { return mTripTotal; }
			uint32_t GetSuppressedCount() const { return mSuppressedTotal; }

		private:
			TripConfig mConfig;
			uint32_t   mEntryIndex;
			uint32_t   mRun;
			uint32_t   mRunStart;
			float      mRunPeak;
			uint32_t   mUnderStreak;
			bool       mArmed;
			bool       mHasEmitted;
			double     mLastEmitMs;
			uint32_t   mSuppressedPending;
			uint32_t   mOverTotal;
			uint32_t   mTripTotal;
			uint32_t   mSuppressedTotal;
		};
	}}
}
