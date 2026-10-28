#include <DiaCellLayers/CellLayerLegend.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::CellLayers
{
	Dia::ValueSchema::LegendData CellLayerLegend::Build(const CellLayerDesc& base, const Dia::ValueSchema::ValueSchemaService* schema,
		Dia::ValueSchema::RampResolver& resolver, bool hasObserved, float observedMin, float observedMax)
	{
		if (schema == nullptr) { return Dia::ValueSchema::LegendData(); }
		const Dia::ValueSchema::ValueInfo* info = schema->Find(base.schemaKey);
		if (info == nullptr)
		{
			const uint32_t key = base.schemaKey.Value();
			bool seen = false;
			for (uint32_t i = 0; i < mWarnedCount; ++i) { if (mWarned[i] == key) { seen = true; break; } }
			if (!seen && mWarnedCount < kMaxWarnedKeys)
			{
				mWarned[mWarnedCount++] = key;
				DIA_LOG_WARNING("CEL", "CellLayerLegend: schema key 0x%x of layer 0x%x not found; empty legend", key, base.id.Value());
			}
			return Dia::ValueSchema::LegendData();
		}
		return Dia::ValueSchema::BuildLegend(resolver, *info, hasObserved, observedMin, observedMax);
	}
}
