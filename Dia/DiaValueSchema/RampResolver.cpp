#include <DiaValueSchema/RampResolver.h>
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaValueSchema/RampSample.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>

#include <cfloat>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>

namespace Dia::ValueSchema
{
	namespace
	{
		constexpr const char* kGreyId = "sequential.grey";
		constexpr const char* kDeltaDefaultId = "diverging.blue_red";
		constexpr const char* kCategoricalId = "categorical.12";

		// double -> float without overflowing to +-inf: out-of-range values saturate at +-FLT_MAX. NaN stays NaN.
		float ClampToFloat(double d)
		{
			if (std::isnan(d)) { return std::numeric_limits<float>::quiet_NaN(); }
			if (d > static_cast<double>(FLT_MAX)) { return FLT_MAX; }
			if (d < -static_cast<double>(FLT_MAX)) { return -FLT_MAX; }
			return static_cast<float>(d);
		}

		RampData BuiltInFallback()
		{
			RampData r;
			SetText(r.mName, "builtin.fallback");
			r.mId = Dia::Core::StringCRC("builtin.fallback");
			r.mKind = RampKind::Continuous;
			r.mStops[0].mT = 0.0f; r.mStops[0].mRgba8 = 0x000000FFu;
			r.mStops[1].mT = 1.0f; r.mStops[1].mRgba8 = 0xFFFFFFFFu;
			r.mStopCount = 2;
			return r;
		}
	}

	struct RampResolver::Impl
	{
		std::unordered_set<std::string> mWarned;
	};

	RampResolver::RampResolver(const ValueSchemaService& service) : mImpl(new Impl()), mService(service) {}
	RampResolver::~RampResolver() { delete mImpl; }

	float RampT(const ResolvedRamp& ramp, float value)
	{
		// Computed in double: the width of a float range such as [-3e38, 3e38] overflows float and would give NaN.
		if (std::isnan(value) || !std::isfinite(ramp.mMin) || !std::isfinite(ramp.mMax)) { return 0.0f; }
		const double width = static_cast<double>(ramp.mMax) - static_cast<double>(ramp.mMin);
		if (!(width > 0.0)) { return 0.0f; }
		double t = (static_cast<double>(value) - static_cast<double>(ramp.mMin)) / width;	// +-inf value gives +-inf, clamped below
		if (t < 0.0) { t = 0.0; }
		if (t > 1.0) { t = 1.0; }
		return static_cast<float>(t);
	}

	void RampResolver::NoteFallback()
	{
		++mFallbackCount;
		if (mCounter) { mCounter->Inc(); }
	}

	bool RampResolver::MarkWarned(const char* tag, const ValueInfo& info)
	{
		return mImpl->mWarned.insert(std::string(tag) + info.mFullKey.AsCStr()).second;
	}

	ResolvedRamp RampResolver::ResolveNamed(const ValueInfo& info, const char* uiKey, const char* defaultId)
	{
		ResolvedRamp out;
		// mUi is an open block: anything other than an object (null, array, string, ...) means "no request".
		const Json::Value* v = info.mUi.isObject() ? &info.mUi[uiKey] : nullptr;
		const bool requested = v != nullptr && v->isString();
		const std::string wanted = requested ? v->asString() : std::string(defaultId);

		const RampData* found = mService.FindRamp(Dia::Core::StringCRC(wanted.c_str()));
		bool unusable = found == nullptr || found->mKind != RampKind::Continuous;
		if (unusable)
		{
			if (requested && MarkWarned(uiKey, info))
			{
				++mUnknownWarnings;
				DIA_LOG_WARNING("VSC", "%s: ui.%s '%s' is not a known continuous ramp, using '%s'",
					info.mFullKey.AsCStr(), uiKey, wanted.c_str(), defaultId);
			}
			found = mService.FindRamp(Dia::Core::StringCRC(defaultId));
			if (found && found->mKind != RampKind::Continuous) { found = nullptr; }
		}
		if (found) { out.mRamp = *found; } else { out.mRamp = BuiltInFallback(); unusable = true; }

		// Fallback only counts when the request could not be honoured (unknown id, or default/grey missing).
		if (unusable) { out.mFellBack = true; NoteFallback(); }
		return out;
	}

	ResolvedRamp RampResolver::ResolveRamp(const ValueInfo& info)
	{
		ResolvedRamp out = ResolveNamed(info, "ramp", kGreyId);
		if (info.mHasMin && info.mHasMax)
		{
			out.mHasRange = true;
			out.mMin = ClampToFloat(info.mMin);
			out.mMax = ClampToFloat(info.mMax);
		}
		return out;
	}

	ResolvedRamp RampResolver::ResolveDeltaRamp(const ValueInfo& info, float maxAbs)
	{
		ResolvedRamp out = ResolveNamed(info, "delta_ramp", kDeltaDefaultId);
		if (!std::isfinite(maxAbs) || !(maxAbs > 0.0f)) { maxAbs = 1.0f; }
		out.mHasRange = true;
		out.mMin = -maxAbs;
		out.mMax = maxAbs;
		return out;
	}

	ResolvedPalette RampResolver::ResolvePalette(const ValueInfo& info)
	{
		ResolvedPalette out;
		if (info.mType != ValueType::Enum) { return out; }

		const RampData* cat = mService.FindRamp(Dia::Core::StringCRC(kCategoricalId));
		if (cat && cat->mKind != RampKind::Categorical) { cat = nullptr; }
		const unsigned count = cat ? cat->mColourCount : kCategoricalColourCount;

		unsigned colourless = 0;
		const unsigned n = info.mEnumValues.Size();
		out.mEntries.Reserve(n > 0 ? n : 1);
		for (unsigned i = 0; i < n; ++i)
		{
			const EnumValue& ev = info.mEnumValues[i];
			PaletteEntry e;
			e.mId = ev.mId;
			uint32_t rgba = 0;
			if (ev.mHasColour && ParseHexColour(ev.mColour.AsCStr(), rgba))
			{
				e.mRgba8 = rgba;
			}
			else
			{
				++colourless;
				if (cat && cat->mColourCount > 0) { e.mRgba8 = RampColourAt(*cat, i); }
				else if (ParseHexColour(GetCategoricalColour(i), rgba)) { e.mRgba8 = rgba; }
				else { e.mRgba8 = out.mFallbackRgba8; }
			}
			out.mEntries.Add(e);
		}

		if (count > 0 && colourless > count)
		{
			out.mWrapped = true;
			if (MarkWarned("palette", info))
			{
				++mWrapWarnings;
				DIA_LOG_WARNING("VSC", "%s: %u colour-less enum values exceed the %u categorical colours; colours repeat",
					info.mFullKey.AsCStr(), colourless, count);
			}
		}
		return out;
	}
}
