#pragma once

// ValueClamper: clamps numeric (float/int) values to a ValueInfo's [min, max] range.
//  - Logs ONE Warning per full key (channel "VSC") the first time that key clamps (or sees NaN).
//  - Counts every clamp (ClampCount) and increments an optional bound metric Counter.
//  - Non-numeric types and range-less entries pass through unchanged. An absent bound is not applied.
//  - NaN passes through unchanged; it warns ("NaN value") once per key but is not counted as a clamp.
//  - No global state (VS-005): the once-per-key memory lives in the instance. Instances are independent.

#include <DiaValueSchema/ValueInfo.h>

namespace Dia::Observation::Metric { class Counter; }

namespace Dia::ValueSchema
{
	class ValueClamper
	{
	public:
		ValueClamper();
		~ValueClamper();
		ValueClamper(const ValueClamper&) = delete;
		ValueClamper& operator=(const ValueClamper&) = delete;

		double Clamp(double value, const ValueInfo& info);
		float  Clamp(float value, const ValueInfo& info);
		int    Clamp(int value, const ValueInfo& info);

		unsigned int ClampCount() const		{ return mClampCount; }		// every clamp
		unsigned int WarnedKeyCount() const;							// distinct keys warned
		unsigned int WarningCount() const	{ return mWarningCount; }	// Warnings emitted

		void BindClampCounter(Dia::Observation::Metric::Counter* counter) { mCounter = counter; }

	private:
		bool MarkWarned(const ValueInfo& info);

		struct Impl;
		Impl*			mImpl;
		unsigned int	mClampCount = 0;
		unsigned int	mWarningCount = 0;
		Dia::Observation::Metric::Counter* mCounter = nullptr;
	};
}
