#include <DiaCellLayers/CellLayerLegend.h>
#include <DiaObservation/Log/DiaLog.h>

#include <string.h>

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

	PresentedLegend CellLayerLegend::BuildPresented(const CellLayerDesc& base, const Dia::ValueSchema::ValueSchemaService* schema,
		Dia::ValueSchema::RampResolver& resolver, bool hasObserved, float observedMin, float observedMax,
		const ViewPresentation* presentation)
	{
		PresentedLegend out;
		out.legend = Build(base, schema, resolver, hasObserved, observedMin, observedMax);
		if (presentation == nullptr) { return out; }
		memcpy(out.unitPrefix, presentation->unitPrefix, sizeof(out.unitPrefix));
		out.unitPrefix[sizeof(out.unitPrefix) - 1] = '\0';
		Dia::ValueSchema::LegendData& l = out.legend;
		if (l.mIsCategory || schema == nullptr) { return out; }
		const Dia::ValueSchema::RampData* ramp = schema->FindRamp(presentation->rampId);
		if (ramp != nullptr && ramp->mKind == Dia::ValueSchema::RampKind::Continuous) { l.mRamp.mRamp = *ramp; }
		l.mRamp.mMin = presentation->rangeMin;
		l.mRamp.mMax = presentation->rangeMax;
		l.mRamp.mHasRange = true;
		l.mMin = presentation->rangeMin;
		l.mMax = presentation->rangeMax;
		l.mTickCount = Dia::ValueSchema::BuildTicks(l.mMin, l.mMax, l.mTicks);
		return out;
	}
}
