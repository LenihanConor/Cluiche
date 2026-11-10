#include <DiaValueSchema/RampSample.h>

#include <cmath>

namespace Dia::ValueSchema
{
	namespace
	{
		constexpr uint32_t kBlack = 0x000000FFu;

		uint32_t Lerp8(uint32_t a, uint32_t b, float f)
		{
			const float v = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * f;
			const long r = std::lround(v);
			return static_cast<uint32_t>(r < 0 ? 0 : (r > 255 ? 255 : r));
		}
	}

	uint32_t SampleRamp(const RampData& ramp, float t)
	{
		if (ramp.mKind == RampKind::Categorical)
		{
			return ramp.mColourCount > 0 ? ramp.mColours[0] : kBlack;
		}
		if (ramp.mStopCount == 0) { return kBlack; }
		if (std::isnan(t) || t < 0.0f) { t = 0.0f; }
		if (t > 1.0f) { t = 1.0f; }

		if (t <= ramp.mStops[0].mT) { return ramp.mStops[0].mRgba8 | 0xFFu; }
		for (unsigned i = 1; i < ramp.mStopCount; ++i)
		{
			const RampStop& hi = ramp.mStops[i];
			if (t <= hi.mT)
			{
				const RampStop& lo = ramp.mStops[i - 1];
				const float span = hi.mT - lo.mT;
				const float f = span > 0.0f ? (t - lo.mT) / span : 1.0f;
				const uint32_t r = Lerp8((lo.mRgba8 >> 24) & 0xFF, (hi.mRgba8 >> 24) & 0xFF, f);
				const uint32_t g = Lerp8((lo.mRgba8 >> 16) & 0xFF, (hi.mRgba8 >> 16) & 0xFF, f);
				const uint32_t b = Lerp8((lo.mRgba8 >> 8) & 0xFF, (hi.mRgba8 >> 8) & 0xFF, f);
				return (r << 24) | (g << 16) | (b << 8) | 0xFFu;
			}
		}
		return ramp.mStops[ramp.mStopCount - 1].mRgba8 | 0xFFu;
	}

	uint32_t RampColourAt(const RampData& ramp, unsigned index)
	{
		if (ramp.mKind != RampKind::Categorical) { return SampleRamp(ramp, 0.0f); }
		if (ramp.mColourCount == 0) { return kBlack; }
		return ramp.mColours[index % ramp.mColourCount];
	}
}
