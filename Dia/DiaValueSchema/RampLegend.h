#pragma once

// RampLegend: builds the neutral legend data (ramp + range + unit + ticks + category swatches) for one schema
// entry. The consumer draws it; this module draws nothing and has no graphics dependency.
//
// Deliberate deviation from the spec signature: takes the RampResolver (which wraps the ValueSchemaService and
// owns the once-per-key warning memory) instead of a ValueSchemaService&.
//
// Float / Int entries : mRamp = ResolveRamp(info); mMin/mMax are the resolved range (0..1 when the entry has none);
//                       mUnit = entry unit; mTicks/mTickCount as below; mIsCategory = false.
// Enum entries        : mIsCategory = true; one swatch per enum value in schema order. Label = value label, or the
//                       value id when the label is empty. Colour = ResolvePalette colour. No ticks.
// Other types         : empty legend (no ticks, no swatches, mIsCategory false).
//
// Seen range: (hasSeen, seenMin, seenMax) is returned as mHasSeen/mSeenMin/mSeenMax. If seenMin > seenMax they are
// swapped. If either is NaN/inf, mHasSeen is false.
//
// Tick algorithm ("nice" ticks, always includes both ends exactly as given, at most 5 ticks):
//   Choose the smallest step of the form {1,2,5} x 10^n so that at most 3 multiples of the step lie STRICTLY
//   between min and max; ticks = [min, those multiples ascending, max]. Interior values are computed as an
//   integer times a power of ten (division for negative powers), so there is no float noise (0.3 not 0.30000000000000004).
// Interior ticks that collapse onto a neighbour after the cast to float (one-ulp ranges, subnormals, > 2^24) are dropped, so
//   ticks are always strictly increasing floats.
// Degenerate input: min == max or min > max (invalid) -> a single tick {min}; NaN/inf bounds -> no ticks.
//   Examples: 0..1 -> 0,0.5,1   0..7 -> 0,2,4,6,7   -1..1 -> -1,-0.5,0,0.5,1   3..17 -> 3,5,10,15,17.

#include <DiaValueSchema/RampResolver.h>

namespace Dia::ValueSchema
{
	constexpr unsigned kMaxLegendTicks = 5;

	struct SwatchEntry
	{
		Dia::Core::Containers::String64		mId;
		Dia::Core::Containers::String128	mLabel;
		uint32_t							mRgba8 = 0;
	};

	struct LegendData
	{
		// ramp legends
		ResolvedRamp						mRamp;
		float								mMin = 0.0f;
		float								mMax = 1.0f;
		Dia::Core::Containers::String32		mUnit;
		float								mTicks[kMaxLegendTicks] = {};
		unsigned							mTickCount = 0;
		bool								mHasSeen = false;
		float								mSeenMin = 0.0f;
		float								mSeenMax = 0.0f;

		// category legends
		ValueList<SwatchEntry>				mSwatches;
		bool								mIsCategory = false;
	};

	// Writes up to kMaxLegendTicks nice ticks for [min,max] (algorithm above); returns the count.
	unsigned BuildTicks(float min, float max, float (&outTicks)[kMaxLegendTicks]);

	LegendData BuildLegend(RampResolver& resolver, const ValueInfo& info, bool hasSeen, float seenMin, float seenMax);
}
