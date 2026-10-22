#pragma once

// ValueSchemaHealth: IHealthReporter "dia.valueschema" over a SchemaLoadStats (AC 11).
//  - Degraded (reason "valueschema.files_failed") when any schema file failed to load.
//  - Also Degraded (reason "valueschema.ramp_files_failed") when a ramp file failed as a whole; a single skipped
//    ramp inside a good file does not degrade health.
//  - Also Degraded (reason "valueschema.attr_registrations_dropped") when the optional component-attribute audit
//    report shows dropped FieldAttributeRegistry registrations (Feature 7). Lower priority than the two above.
//  - OK otherwise. errors = files failed (schema + ramp), warnings = missing roots.

#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaValueSchema/SchemaDirectoryLoader.h>
#include <DiaValueSchema/RampSetLoader.h>
#include <DiaValueSchema/ComponentAttributeAudit.h>

namespace Dia::ValueSchema
{
	class ValueSchemaHealth : public Dia::Observation::Health::IHealthReporter
	{
	public:
		explicit ValueSchemaHealth(const SchemaLoadStats& stats, const RampLoadStats* rampStats = nullptr,
			const ComponentAttributeAuditReport* auditReport = nullptr)
			: mStats(stats), mRampStats(rampStats), mAuditReport(auditReport) {}

		Dia::Core::StringCRC				GetReporterName() const override;
		Dia::Observation::Health::Health	Report() const override;

	private:
		const SchemaLoadStats&	mStats;
		const RampLoadStats*	mRampStats;
		const ComponentAttributeAuditReport*	mAuditReport;
	};
}
