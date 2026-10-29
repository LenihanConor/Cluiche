#include <DiaCellLayers/SelectionStats.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

namespace Dia::CellLayers::SelectionStats
{
	namespace
	{
		struct Accum
		{
			const LayerValues*	values = nullptr;
			uint32_t			count = 0;
			uint32_t			nanCount = 0;
			float				min = 0.0f;
			float				max = 0.0f;
			double				sum = 0.0;
		};

		void Visit(uint32_t cellIndex, void* user)
		{
			Accum& a = *static_cast<Accum*>(user);
			if (cellIndex >= a.values->count) { return; }
			const float v = a.values->values[cellIndex];
			if (v != v) { ++a.nanCount; return; }
			if (a.count == 0) { a.min = v; a.max = v; }
			else
			{
				if (v < a.min) { a.min = v; }
				if (v > a.max) { a.max = v; }
			}
			a.sum += static_cast<double>(v);
			++a.count;
		}
	}

	void ComputeFloat(const ICellSelectionView& selection, const LayerValues& values, LayerStats& out)
	{
		out.hasData = false;
		out.count = 0;
		out.nanCount = 0;
		out.hasRange = false;
		out.min = 0.0f;
		out.max = 0.0f;
		out.mean = 0.0;
		if (!values.hasData || values.values == nullptr) { return; }

		Accum a;
		a.values = &values;
		selection.ForEach(&Visit, &a);

		out.hasData = true;
		out.count = a.count;
		out.nanCount = a.nanCount;
		if (a.count > 0)
		{
			out.hasRange = true;
			out.min = a.min;
			out.max = a.max;
			out.mean = a.sum / static_cast<double>(a.count);
		}
	}

	bool Compute(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
		const ICellSelectionView& selection, Dia::Core::StringCRC layerId, LayerStats& out)
	{
		out = LayerStats();
		out.layerId = layerId;
		const CellLayerDesc* desc = registry.Find(layerId);
		if (desc == nullptr) { return false; }

		const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(desc->schemaKey) : nullptr;
		if (info != nullptr)
		{
			Dia::ValueSchema::SetText(out.label, info->mLabel.AsCStr());
			Dia::ValueSchema::SetText(out.unit, info->mUnit.AsCStr());
		}

		ComputeFloat(selection, reader.Read(layerId), out);
		return out.hasData;
	}
}
