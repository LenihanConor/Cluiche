#include <DiaCellHistory/DeltaScale.h>

#include <cmath>

namespace Dia::CellHistory
{
	float DeltaScale::NiceCeil(float v)
	{
		if (!std::isfinite(v)) { return kDefault; }
		if (v <= 0.0f) { return kFloor; }
		const double e = std::floor(std::log10(static_cast<double>(v)));
		const double p = std::pow(10.0, e);
		static const double kSteps[] = { 1.0, 2.0, 5.0, 10.0 };
		for (double m : kSteps)
		{
			const float c = static_cast<float>(m * p);	// compared as floats so an exact power of ten does not drift up
			if (c >= v) { return c; }
		}
		return static_cast<float>(10.0 * p);
	}

	void DeltaScale::Reset()
	{
		mHasValue = false;
		mScale = kDefault;
		mLow = false;
		mLowSince = 0.0;
	}

	float DeltaScale::Observe(float maxAbsDelta, double nowSeconds)
	{
		if (!std::isfinite(maxAbsDelta) || maxAbsDelta < 0.0f) { return Current(); }

		if (!mHasValue)
		{
			mHasValue = true;
			mScale = (maxAbsDelta == 0.0f) ? kDefault : NiceCeil(maxAbsDelta);
			if (mScale < kFloor) { mScale = kFloor; }
			mLow = false;
			return mScale;
		}

		float target = (maxAbsDelta == 0.0f) ? kFloor : NiceCeil(maxAbsDelta);
		if (target < kFloor) { target = kFloor; }
		if (target > mScale)
		{
			mScale = target;	// grow immediately, nothing is clipped
			mLow = false;
		}
		else if (target < mScale && maxAbsDelta < kShrinkFraction * mScale)
		{
			if (!mLow) { mLow = true; mLowSince = nowSeconds; }
			if (nowSeconds - mLowSince >= kShrinkSeconds)
			{
				mScale = target;
				mLow = false;
			}
		}
		else
		{
			mLow = false;	// between 40% and 100% of the scale: hold and cancel any timer
		}
		return mScale;
	}
}
