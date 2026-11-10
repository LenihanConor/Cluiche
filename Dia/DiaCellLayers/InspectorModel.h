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
//
// Stats JSON: BuildStatsJson builds the `cells.inspect` result without any card or domain (Feature 5's select-stats query reuses
// it). `request` picks the layers: Shown (the InspectorState's shown set, base first), Group(name) (the active grid's layers whose
// group is name), Layers(ids...) (in request order) or All (every layer of the active grid). Stats are computed fresh through the
// LayerReader (at most one read per layer version) and never touch the InspectorState's cache or pass count, so the answer is
// never stale even before the next viewer Tick. Empty selection (null or Count() == 0): nothing is computed or read and the
// result is {"ok":true,"selected":0,"grid":..,"updating":..,"layers":[]} (plus "pinned" when A is pinned). A group that has no
// layer on the active grid, an unknown layer id or a layer of another grid returns false and sets out = {"error": text}.
// Result keys (all stats are exact: floats as doubles, no rounding):
//   ok, selected, grid{width,height}, updating, base (schema key text of the active base), histogram (float base: {bins[16],
//   minRange, maxRange, valid, outOfRangeLow, outOfRangeHigh, count, nanCount}; category base: {categories[],other,nanCount}),
//   layers[{layer (layer id text), schemaKey (full schema key text, from the schema when found, else the registered key),
//   kind "float"|"category", label, unit, base (true for the active base), hasData, count, nanCount, min / mean / max (only
//   with a range), categories[{id,label,count}] / other / truncated (category), a {..same stats..} / delta{count,min,mean,max}
//   (only when A is pinned; a is {"notFrozen":true} for a layer registered after a freeze, {"notComputed":true} for a layer outside
//   the shown set while A is live; delta omits min / mean / max when a side has no range)}], pinned{count,frozen} (when pinned),
//   cell{index,q,r,rows[{layer,schemaKey,label,unit,kind,hasData,isNaN,value (omitted when NaN / no data),otherValue,enumId,
//   enumLabel}],details[{kind "section"|"value"|"text",label,..}],detailsTruncated} (exactly one cell selected; the card is
//   treated as open since this is an explicit request; rows cover the SHOWN set, as in BuildSingleCell).

#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCellLayers/InspectorState.h>

#include <json/json.h>
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

	// Which layers BuildStatsJson reports.
	struct InspectRequest
	{
		enum class Mode : uint8_t { Shown, Group, Layers, All };
		static constexpr uint32_t kMaxLayers = CellLayerRegistry::kMaxLayers;

		Mode								mode = Mode::Shown;
		Dia::Core::Containers::String32		group;			// Mode::Group
		uint32_t							layerCount = 0;	// Mode::Layers
		Dia::Core::StringCRC				layers[kMaxLayers];

		bool AddLayer(Dia::Core::StringCRC id) { if (layerCount >= kMaxLayers) { return false; } layers[layerCount++] = id; return true; }
	};

	namespace InspectorModel
	{
		bool BuildSingleCell(CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const InspectorState& state, const ICellSelectionView* selection, bool cardOpen, SingleCellTable& out);

		// The panel card's state (GetJSONState "inspector"), built from InspectorState's CACHED stats: no layer is recomputed or read
		// for the aggregates (the single-cell block reads through the LayerReader, once per layer version, and only when exactly one
		// cell is selected). `out` is replaced with: {hasSelection, updating, expandedGroups[text], base (schema key text; with a
		// selection), layers[{id, schemaKey, label, unit, tooltip (schema description, else hint), kind, group, hasData, count,
		// nanCount, min / mean / max (null without a range or for category layers), rangeMin / rangeMax (schema range, only when the
		// schema has one), categories[{id,label,count}] / other (category layers), a {..stats..} | {notFrozen:true} |
		// {notComputed:true} and delta{count,min,mean,max} (min / mean / max null when a side has no range) when A is pinned}] in
		// shown-set order (base first), histogram (as in the Stats JSON), pinned{count,frozen} (when pinned), cell{..as in the Stats JSON..}}.
		// Without a selection `layers` is empty and nothing is computed or read.
		void BuildCardJson(CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const InspectorState& state, const ICellSelectionView* selection, Json::Value& out);

		// See "Stats JSON" above. Returns false (out = {"error"}) on a bad request; `out` is replaced.
		bool BuildStatsJson(CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const InspectorState& state, const ICellSelectionView* selection, const InspectRequest& request, Json::Value& out);
	}
}
