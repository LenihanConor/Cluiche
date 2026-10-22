#include <DiaValueSchema/RampResolver.h>
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaValueSchema/RampSample.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>

#include <cmath>
#include <string>
#include <unordered_set>

namespace Dia::ValueSchema
{
	namespace
	{
		constexpr const char* kGreyId = "sequential.grey";
		constexpr const char* kDeltaDefaultId = "diverging.blue_red";
		constexpr const char* kCategoricalId = "categorical.12";

		RampData BuiltInFallback()
		{
			RampData r;
			r.mName = "builtin.fallback";
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
		const float width = ramp.mMax - ramp.mMin;
		if (!(width > 0.0f) || std::isnan(value)) { return 0.0f; }
		float t = (value - ramp.mMin) / width;
		if (t < 0.0f) { t = 0.0f; }
		if (t > 1.0f) { t = 1.0f; }
		return t;
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
		const Json::Value& v = info.mUi[uiKey];
		const bool requested = v.isString();
		const std::string wanted = requested ? v.asString() : std::string(defaultId);

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
			out.mMin = static_cast<float>(info.mMin);
			out.mMax = static_cast<float>(info.mMax);
		}
		return out;
	}

	ResolvedRamp RampResolver::ResolveDeltaRamp(const ValueInfo& info, float maxAbs)
	{
		ResolvedRamp out = ResolveNamed(info, "delta_ramp", kDeltaDefaultId);
		if (std::isnan(maxAbs) || !(maxAbs > 0.0f)) { maxAbs = 1.0f; }
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
