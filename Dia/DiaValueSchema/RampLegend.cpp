#include <DiaValueSchema/RampLegend.h>

#include <cmath>

namespace Dia::ValueSchema
{
	namespace
	{
		constexpr unsigned kMaxInteriorTicks = 3;

		// k * m * 10^e as a double without accumulated noise: integer arithmetic, then one correctly rounded
		// multiply (e >= 0) or divide (e < 0).
		double NiceValue(long long k, int m, int e)
		{
			const double scaled = static_cast<double>(k) * static_cast<double>(m);
			return e >= 0 ? scaled * std::pow(10.0, e) : scaled / std::pow(10.0, -e);
		}

		// Interior multiples of (m * 10^e) strictly between min and max, ascending. Returns the count found,
		// stopping once more than `cap` were found (the caller only needs to know it is too many).
		unsigned CollectInterior(double mn, double mx, int m, int e, unsigned cap, float* out)
		{
			const double step = NiceValue(1, m, e);
			const double eps = (mx - mn) * 1e-6;
			const long long kLo = static_cast<long long>(std::floor(mn / step)) - 1;
			const long long kHi = static_cast<long long>(std::ceil(mx / step)) + 1;
			unsigned n = 0;
			for (long long k = kLo; k <= kHi; ++k)
			{
				const double v = NiceValue(k, m, e);
				if (v > mn + eps && v < mx - eps)
				{
					if (n >= cap) { return cap + 1; }
					if (out) { out[n] = static_cast<float>(v); }
					++n;
				}
			}
			return n;
		}
	}

	unsigned BuildTicks(float min, float max, float (&outTicks)[kMaxLegendTicks])
	{
		if (!std::isfinite(min) || !std::isfinite(max)) { return 0; }
		if (!(max > min))
		{
			outTicks[0] = min;
			return 1;
		}

		const double mn = static_cast<double>(min);
		const double mx = static_cast<double>(max);
		const int e0 = static_cast<int>(std::floor(std::log10(mx - mn))) - 1;
		static const int kMantissas[3] = { 1, 2, 5 };

		float interior[kMaxInteriorTicks + 1];
		unsigned count = 0;
		for (int e = e0; e <= e0 + 4; ++e)
		{
			bool found = false;
			for (int mi = 0; mi < 3 && !found; ++mi)
			{
				if (CollectInterior(mn, mx, kMantissas[mi], e, kMaxInteriorTicks, nullptr) <= kMaxInteriorTicks)
				{
					count = CollectInterior(mn, mx, kMantissas[mi], e, kMaxInteriorTicks, interior);
					found = true;
				}
			}
			if (found) { break; }
		}

		// The interior values were chosen in double precision; after the cast to float two of them (or one and an end)
		// can coincide (one-ulp ranges, subnormals, > 2^24). Keep only ticks strictly between the previous tick and max.
		unsigned n = 0;
		outTicks[n++] = min;
		for (unsigned i = 0; i < count; ++i)
		{
			if (interior[i] > outTicks[n - 1] && interior[i] < max) { outTicks[n++] = interior[i]; }
		}
		outTicks[n++] = max;
		return n;
	}

	LegendData BuildLegend(RampResolver& resolver, const ValueInfo& info, bool hasSeen, float seenMin, float seenMax)
	{
		LegendData out;

		if (info.mType == ValueType::Float || info.mType == ValueType::Int)
		{
			out.mRamp = resolver.ResolveRamp(info);
			out.mMin = out.mRamp.mMin;
			out.mMax = out.mRamp.mMax;
			out.mUnit = info.mUnit;
			out.mTickCount = BuildTicks(out.mMin, out.mMax, out.mTicks);

			if (hasSeen && std::isfinite(seenMin) && std::isfinite(seenMax))
			{
				out.mHasSeen = true;
				out.mSeenMin = seenMin <= seenMax ? seenMin : seenMax;
				out.mSeenMax = seenMin <= seenMax ? seenMax : seenMin;
			}
		}
		else if (info.mType == ValueType::Enum)
		{
			out.mIsCategory = true;
			const ResolvedPalette pal = resolver.ResolvePalette(info);
			const unsigned n = info.mEnumValues.Size();
			out.mSwatches.Reserve(n > 0 ? n : 1);
			for (unsigned i = 0; i < n; ++i)
			{
				const EnumValue& ev = info.mEnumValues[i];
				SwatchEntry s;
				s.mId = ev.mId;
				s.mLabel = ev.mLabel.AsCStr()[0] != '\0' ? ev.mLabel : Dia::Core::Containers::String128("%s", ev.mId.AsCStr());	// "%s": the const char* ctor is printf-style
				s.mRgba8 = i < pal.mEntries.Size() ? pal.mEntries[i].mRgba8 : pal.mFallbackRgba8;
				out.mSwatches.Add(s);
			}
		}
		return out;
	}
}
