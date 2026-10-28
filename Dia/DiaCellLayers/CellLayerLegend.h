#pragma once

// CellLayerLegend: builds the panel-card legend for the active base layer. Wraps Dia::ValueSchema::BuildLegend:
// float layers get the schema ramp/range/unit/ticks plus the OBSERVED min/max; category layers get one swatch per enum
// value. The caller owns the RampResolver (one per consumer thread) and supplies the observed range, typically from
// LayerUploader::ObservedMin/Max. Generic: no game types (CL-005), no singletons (CL-006). Single-thread use.
//
// Unresolved schemaKey -> empty LegendData and one Warning per key on channel CEL (fixed memory of kMaxWarnedKeys;
// beyond that, further distinct keys are not warned about). Null schema -> empty LegendData, no warning.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/IViewValueSource.h>
#include <DiaValueSchema/RampLegend.h>

namespace Dia::CellLayers
{
	// A legend plus the unit prefix a view source supplied (empty without a presentation).
	struct PresentedLegend
	{
		Dia::ValueSchema::LegendData	legend;
		char							unitPrefix[8] = {};
	};

	class CellLayerLegend
	{
	public:
		static const uint32_t kMaxWarnedKeys = 32;

		CellLayerLegend() = default;
		CellLayerLegend(const CellLayerLegend&) = delete;
		CellLayerLegend& operator=(const CellLayerLegend&) = delete;

		Dia::ValueSchema::LegendData Build(const CellLayerDesc& base, const Dia::ValueSchema::ValueSchemaService* schema,
			Dia::ValueSchema::RampResolver& resolver, bool hasObserved, float observedMin, float observedMax);

		// As Build, but a non-null presentation (float layers) replaces the ramp (if its id is known to the schema), the
		// range (ticks recomputed) and adds the unit prefix.
		PresentedLegend BuildPresented(const CellLayerDesc& base, const Dia::ValueSchema::ValueSchemaService* schema,
			Dia::ValueSchema::RampResolver& resolver, bool hasObserved, float observedMin, float observedMax,
			const ViewPresentation* presentation);

		uint32_t	UnresolvedWarningCount() const { return mWarnedCount; }

	private:
		uint32_t	mWarned[kMaxWarnedKeys] = {};
		uint32_t	mWarnedCount = 0;
	};
}
