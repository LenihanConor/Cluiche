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
		h.errors = mStats.mFilesFailed;
		h.warnings = mStats.mRootsMissing;
		if (mStats.mFilesFailed > 0)
		{
			h.status = Dia::Observation::Health::HealthStatus::kDegraded;
			h.reason = Dia::Core::StringCRC("valueschema.files_failed");
		}
		else
		{
			h.status = Dia::Observation::Health::HealthStatus::kOK;
			h.reason = Dia::Core::StringCRC::kZero;
		}
		return h;
	}
}
