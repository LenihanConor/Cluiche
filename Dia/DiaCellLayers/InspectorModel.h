#pragma once

// InspectorModel: builds the Selection Inspector's data (Feature 4) independent of any UI or viewer domain, so the card, the
// JSON output and the select-stats query can all reuse it (CL-005 generic, CL-006 no singletons, PD-004 no STL in the API).
// Everything it needs is passed by reference / pointer per call. Runs on the viewer's processing unit; not thread-safe.
//
// Single cell: BuildSingleCell fills a SingleCellTable only when ShouldRunProviders(selection, cardOpen) holds (exactly one
// cell selected and the card open); otherwise it returns false, clears `out`, calls no provider and reads no layer. The table
// has one row per layer in the InspectorState's shown set (ShownLayerAt order, index 0 = the active base), values read through
// the LayerReader (so each layer is read at most once per version), followed by the details rows built by the active grid's
// providers in registration order (called exactly once). Values are kept as exact floats; formatting is the caller's job.
// Category layers also give the enum id and label of the value (value = index in the schema enum); a value outside the enum
// (negative, fractional, too large) sets otherValue and leaves the id / label empty. A NaN value sets isNaN (hasData true);
// a layer with no data, or a cell index past the layer's value count, sets hasData false.
// The registry is non-const because running the providers (Details().Build) updates their overflow-warning state.
// q / r come from the active grid width (row-major: index = r * width + q).

#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCellLayers/InspectorState.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	struct SingleCellRow
	{
		Dia::Core::StringCRC				layerId;
		Dia::Core::Containers::String128	label;			// schema label
		Dia::Core::Containers::String32		unit;			// schema unit
		float								value = 0.0f;	// exact, as read
		bool								isCategory = false;
		bool								hasData = false;
		bool								isNaN = false;
		bool								otherValue = false;	// category value outside the schema enum
		Dia::Core::Containers::String64		enumId;			// category layers, value inside the enum
		Dia::Core::Containers::String128	enumLabel;
	};

	class SingleCellTable
	{
	public:
		SingleCellTable();
		~SingleCellTable();
		SingleCellTable(const SingleCellTable&) = delete;
		SingleCellTable& operator=(const SingleCellTable&) = delete;

		void				Clear();	// rows, details and coordinates
		uint32_t			RowCount() const { return mRowCount; }
		const SingleCellRow&	Row(uint32_t index) const { return mRows[index]; }	// index < RowCount()
		SingleCellRow*		AddRow();	// null when full (InspectorState::kMaxShown)

		uint32_t			cellIndex = 0;
		uint32_t			q = 0;
		uint32_t			r = 0;
		CellDetailResult	details;

	private:
		SingleCellRow*		mRows;
		uint32_t			mRowCount = 0;
	};

	namespace InspectorModel
	{
		bool BuildSingleCell(CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const InspectorState& state, const ICellSelectionView* selection, bool cardOpen, SingleCellTable& out);
	}
}
