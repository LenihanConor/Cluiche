#pragma once

// DeltaScale: the auto colour scale for delta mode (Feature 4). Pure logic; time is injected, no clock inside.
// The scale is the largest absolute delta rounded UP to a "nice" number (1, 2 or 5 x 10^n). It grows immediately (nothing is
// clipped) and shrinks only after the largest absolute delta has stayed under 40% of the current scale for 2 seconds.
// Reset() makes the scale undefined until the next observation (a new delta was entered). Non-finite or negative maxima are
// ignored. A maximum of 0 (all deltas zero) is observed as kFloor and thereafter the scale never goes below kFloor.
// No singletons.

namespace Dia::CellHistory
{
	class DeltaScale
	{
	public:
		static constexpr float	kFloor = 1e-6f;			// smallest scale ever produced; also the scale for an all-zero delta
		static constexpr float	kDefault = 1.0f;		// Current() before any observation
		static constexpr float	kShrinkFraction = 0.4f;	// shrink only when max < 40% of the scale...
		static constexpr double	kShrinkSeconds = 2.0;	// ...for this long

		// Smallest of {1,2,5} x 10^n that is >= v. v <= 0 gives kFloor; NaN or inf gives kDefault.
		static float NiceCeil(float v);

		void	Reset();
		float	Observe(float maxAbsDelta, double nowSeconds);	// returns Current()
		float	Current() const { return mHasValue ? mScale : kDefault; }
		bool	HasValue() const { return mHasValue; }

	private:
		bool	mHasValue = false;
		float	mScale = kDefault;
		bool	mLow = false;
		double	mLowSince = 0.0;
	};
}
