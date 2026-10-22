#include <DiaValueSchema/ValueSchemaHealth.h>

namespace Dia::ValueSchema
{
	Dia::Core::StringCRC ValueSchemaHealth::GetReporterName() const
	{
		return Dia::Core::StringCRC("dia.valueschema");
	}

	Dia::Observation::Health::Health ValueSchemaHealth::Report() const
	{
		Dia::Observation::Health::Health h{};
		const unsigned int rampFailed = mRampStats ? mRampStats->mFilesFailed : 0u;
		h.errors = mStats.mFilesFailed + rampFailed;
		h.warnings = mStats.mRootsMissing;
		if (mStats.mFilesFailed > 0)
		{
			h.status = Dia::Observation::Health::HealthStatus::kDegraded;
			h.reason = Dia::Core::StringCRC("valueschema.files_failed");
		}
		else if (rampFailed > 0)
		{
			h.status = Dia::Observation::Health::HealthStatus::kDegraded;
			h.reason = Dia::Core::StringCRC("valueschema.ramp_files_failed");
		}
		else
		{
			h.status = Dia::Observation::Health::HealthStatus::kOK;
			h.reason = Dia::Core::StringCRC::kZero;
		}
		return h;
	}
}
