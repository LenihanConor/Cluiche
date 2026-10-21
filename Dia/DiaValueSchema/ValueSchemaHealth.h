#pragma once

// ValueSchemaHealth: IHealthReporter "dia.valueschema" over a SchemaLoadStats (AC 11).
//  - Degraded (reason "valueschema.files_failed") when any schema file failed to load.
//  - OK otherwise. errors = files failed, warnings = missing roots.

#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaValueSchema/SchemaDirectoryLoader.h>

namespace Dia::ValueSchema
{
	class ValueSchemaHealth : public Dia::Observation::Health::IHealthReporter
	{
	public:
		explicit ValueSchemaHealth(const SchemaLoadStats& stats) : mStats(stats) {}

		Dia::Core::StringCRC				GetReporterName() const override;
		Dia::Observation::Health::Health	Report() const override;

	private:
		const SchemaLoadStats& mStats;
	};
}
