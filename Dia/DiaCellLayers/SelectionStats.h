#pragma once

// SelectionStats: per-layer aggregates over a cell selection (Selection Inspector, Feature 4).
// Float layers: count (non-NaN selected cells), nanCount, min / mean / max, and label + unit from the layer's schema entry.
// Mean uses a double accumulator. count == 0 (empty selection, all-NaN, or no data) reports hasRange == false and no
// min / mean / max. Selected cells with index >= values.count are ignored (neither counted nor NaN).
// Reads go through LayerReader, so each layer is read at most once per version.
// Category layers: one count per enum value in schema order (categoryCounts[i] for enum index i, the numeric cell value),
// otherCount (finite ids outside the enum, plus negative / non-integer / infinite values) and nanCount. Capacity is
// kMaxCategories; enum values beyond it are not itemised - cells holding them are added to otherCount and
// categoryTruncated is set. count == sum(categoryCounts) + otherCount (in-range selected, non-NaN cells).
// Histogram: float base layers use ComputeHistogram (16 equal bins over a min..max range, normally the schema range).
// Values outside the range are clamped into the first / last bin AND counted in outOfRangeLow / outOfRangeHigh (the
// "out of range" footer). A value equal to max lands in the last bin. NaN is excluded (not in bins or footer).
// A degenerate range (max <= min or non-finite) sets valid == false and puts every non-NaN value in bin 0 with no
// out-of-range counts. For a category base layer the category counts in LayerStats serve as the histogram.
// Generic: no game types (CL-005), no singletons (CL-006). No STL in the API (PD-004).

#include <DiaCellLayers/ICellSelectionView.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>

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

		// Category layers only (all zero for float layers).
		static constexpr uint32_t			kMaxCategories = 64;
		uint32_t							categoryCount = 0;	// enum values itemised (<= kMaxCategories)
		bool								categoryTruncated = false;	// the schema enum has more than kMaxCategories values
		uint32_t							otherCount = 0;		// selected cells whose id is outside the enum
		uint32_t							categoryCounts[kMaxCategories] = {};
		Dia::Core::Containers::String64		categoryIds[kMaxCategories];	// enum ids in schema order (filled by Compute)
		Dia::Core::Containers::String128	categoryLabels[kMaxCategories];	// enum labels in schema order (filled by Compute)
	};

	struct Histogram
	{
		static constexpr uint32_t			kBins = 16;
		bool								hasData = false;	// false: the layer has no data in this view
		bool								valid = false;		// false: degenerate range (see header comment)
		float								minRange = 0.0f;
		float								maxRange = 0.0f;
		uint32_t							bins[kBins] = {};
		uint32_t							outOfRangeLow = 0;	// values < minRange (also counted in bins[0])
		uint32_t							outOfRangeHigh = 0;	// values > maxRange (also counted in bins[kBins - 1])
		uint32_t							count = 0;			// sum of bins (non-NaN selected cells)
		uint32_t							nanCount = 0;
	};

	namespace SelectionStats
	{
		// Pure aggregate over a values buffer. Does not touch layerId, label or unit of out. Resets the other fields.
		void ComputeFloat(const ICellSelectionView& selection, const LayerValues& values, LayerStats& out);

		// Pure category aggregate. enumValueCount = number of enum values (ids 0..n-1). Resets count, nanCount, hasData and
		// the category counts; does not touch layerId, label, unit, categoryIds or categoryLabels. hasRange stays false.
		void ComputeCategory(const ICellSelectionView& selection, const LayerValues& values, uint32_t enumValueCount, LayerStats& out);

		// Pure 16-bin histogram over [minRange, maxRange]. Resets out.
		void ComputeHistogram(const ICellSelectionView& selection, const LayerValues& values, float minRange, float maxRange, Histogram& out);

		// Histogram of a float layer over the schema min / max. Returns false (out reset, hasData false) when the layer is not
		// registered, not a Float layer, has no schema min / max, or has no data.
		bool ComputeHistogram(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const ICellSelectionView& selection, Dia::Core::StringCRC layerId, Histogram& out);

		// Reads layerId through the reader, aggregates, and fills layerId, label and unit (schema may be null).
		// Dispatches on the registered kind: Float -> ComputeFloat; Category -> ComputeCategory with the schema enum
		// (ids / labels filled; with no schema entry the enum is empty so every cell counts as other).
		// Returns false (out has layerId set, hasData false) when the layer is not registered or has no data.
		bool Compute(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const ICellSelectionView& selection, Dia::Core::StringCRC layerId, LayerStats& out);
	}
}
