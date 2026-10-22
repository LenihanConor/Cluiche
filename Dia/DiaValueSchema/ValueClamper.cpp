#include <DiaValueSchema/ValueClamper.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>

#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>

namespace Dia::ValueSchema
{
	struct ValueClamper::Impl
	{
		std::unordered_set<std::string> mWarned;
	};

	ValueClamper::ValueClamper() : mImpl(new Impl()) {}
	ValueClamper::~ValueClamper() { delete mImpl; }

	unsigned int ValueClamper::WarnedKeyCount() const { return static_cast<unsigned int>(mImpl->mWarned.size()); }

	bool ValueClamper::MarkWarned(const ValueInfo& info)
	{
		if (!mImpl->mWarned.insert(info.mFullKey.AsCStr()).second) { return false; }
		++mWarningCount;
		return true;
	}

	double ValueClamper::Clamp(double value, const ValueInfo& info)
	{
		if (info.mType != ValueType::Float && info.mType != ValueType::Int) { return value; }
		if (!info.mHasMin && !info.mHasMax) { return value; }
		if (std::isnan(value))
		{
			if (MarkWarned(info)) { DIA_LOG_WARNING("VSC", "%s: NaN value", info.mFullKey.AsCStr()); }
			return value;
		}

		double out = value;
		if (info.mHasMin && out < info.mMin) { out = info.mMin; }
		if (info.mHasMax && out > info.mMax) { out = info.mMax; }
		if (out == value) { return value; }

		++mClampCount;
		if (mCounter) { mCounter->Inc(); }
		if (MarkWarned(info))
		{
			DIA_LOG_WARNING("VSC", "%s = %g outside [%g, %g], clamped", info.mFullKey.AsCStr(), value,
				info.mHasMin ? info.mMin : -INFINITY, info.mHasMax ? info.mMax : INFINITY);
		}
		return out;
	}

	float ValueClamper::Clamp(float value, const ValueInfo& info)
	{
		float out = static_cast<float>(Clamp(static_cast<double>(value), info));
		// Rounding the clamped double back to float can step outside the bounds (0.1f > 0.1, 0.7f < 0.7): move one
		// float towards the interior so the result satisfies the bounds as doubles. Max is applied last, like the
		// double path, so min > max still yields max.
		if ((info.mType == ValueType::Float || info.mType == ValueType::Int) && !std::isnan(out))
		{
			if (info.mHasMin && static_cast<double>(out) < info.mMin) { out = std::nextafter(out, std::numeric_limits<float>::infinity()); }
			if (info.mHasMax && static_cast<double>(out) > info.mMax) { out = std::nextafter(out, -std::numeric_limits<float>::infinity()); }
		}
		return out;
	}

	int ValueClamper::Clamp(int value, const ValueInfo& info)
	{
		const double out = Clamp(static_cast<double>(value), info);
		if (out == static_cast<double>(value)) { return value; }
		// A bound outside the int range would make the cast undefined: saturate instead.
		if (out >= 2147483647.0) { return std::numeric_limits<int>::max(); }
		if (out <= -2147483648.0) { return std::numeric_limits<int>::min(); }
		return static_cast<int>(std::lround(out));
	}
}
