#include <DiaValueSchema/ValueClamper.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>

#include <cmath>
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
		return static_cast<float>(Clamp(static_cast<double>(value), info));
	}

	int ValueClamper::Clamp(int value, const ValueInfo& info)
	{
		const double out = Clamp(static_cast<double>(value), info);
		if (out == static_cast<double>(value)) { return value; }
		return static_cast<int>(std::lround(out));
	}
}
