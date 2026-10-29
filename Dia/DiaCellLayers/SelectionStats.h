#pragma once

// SelectionStats: per-layer aggregates over a cell selection (Selection Inspector, Feature 4).
// Float layers: count (non-NaN selected cells), nanCount, min / mean / max, and label + unit from the layer's schema entry.
// Mean uses a double accumulator. count == 0 (empty selection, all-NaN, or no data) reports hasRange == false and no
// min / mean / max. Selected cells with index >= values.count are ignored (neither counted nor NaN).
// Reads go through LayerReader, so each layer is read at most once per version. Category fields are added in a later task.
// Generic: no game types (CL-005), no singletons (CL-006). No STL in the API (PD-004).

#include <DiaCellLayers/ICellSelectionView.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String32.h>

#include <stdint.h>

namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellLayers
{
	struct LayerStats
	{
		Dia::Core::StringCRC				layerId;
		bool								hasData = false;	// false: the layer has no data in this view (all other fields are defaults)
		uint32_t							count = 0;			// selected, non-NaN cells
		uint32_t							nanCount = 0;		// selected NaN cells
		bool								hasRange = false;	// true iff count > 0; min / mean / max valid only then
		float								min = 0.0f;
		float								max = 0.0f;
		double								mean = 0.0;
		Dia::Core::Containers::String128	label;				// schema label; empty when the schema or key is missing
		Dia::Core::Containers::String32		unit;				// schema unit; empty when the schema or key is missing
	};

	namespace SelectionStats
	{
		// Pure aggregate over a values buffer. Does not touch layerId, label or unit of out. Resets the other fields.
		void ComputeFloat(const ICellSelectionView& selection, const LayerValues& values, LayerStats& out);

		// Reads layerId through the reader, aggregates, and fills layerId, label and unit (schema may be null).
		// Returns false (out has layerId set, hasData false) when the layer is not registered or has no data.
		bool Compute(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const ICellSelectionView& selection, Dia::Core::StringCRC layerId, LayerStats& out);
	}
}
