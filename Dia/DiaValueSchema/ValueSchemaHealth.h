#pragma once

// ValueSchemaHealth: IHealthReporter "dia.valueschema" over a SchemaLoadStats (AC 11).
//  - Degraded (reason "valueschema.files_failed") when any schema file failed to load.
//  - Also Degraded (reason "valueschema.ramp_files_failed") when a ramp file failed as a whole; a single skipped
//    ramp inside a good file does not degrade health.
//  - OK otherwise. errors = files failed (schema + ramp), warnings = missing roots.

#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaValueSchema/SchemaDirectoryLoader.h>
#include <DiaValueSchema/RampSetLoader.h>

namespace Dia::ValueSchema
{
	class ValueSchemaHealth : public Dia::Observation::Health::IHealthReporter
	{
	public:
		explicit ValueSchemaHealth(const SchemaLoadStats& stats, const RampLoadStats* rampStats = nullptr)
			: mStats(stats), mRampStats(rampStats) {}

		Dia::Core::StringCRC				GetReporterName() const override;
		Dia::Observation::Health::Health	Report() const override;

	private:
		const SchemaLoadStats&	mStats;
		const RampLoadStats*	mRampStats;
	};
}
