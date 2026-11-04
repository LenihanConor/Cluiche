#pragma once

// DeltaCardState (Feature 4, task 4.8): the typed data behind the delta-mode part of the history card, exposed by CellHistoryDomain
// as the "delta" object of its state (absent outside delta mode). Owns the GroupDeltaBuilder and GroupDeltaTable.
//
//   "delta": { "layer": "<id>", "a": "<side>", "b": "<side>",
//              "groups": { "side": "A"|"B",            // which side's grouping defines the cells
//                          "selected": <n>,            // selected cell count; 0 = no selection, the table covers the whole grid
//                          "baseNoData": bool, "truncated": bool,
//                          "groups": [ { "layer", "value", "label"?, "count", "meanA", "meanB", "change" } ],   // |change| descending
//                          "usedLive": [ "<grouping layer id>" ... ] },   // membership came from live values (chosen side had none)
//              "hover": null | { "cell", "q", "r", "a", "b", "delta", "aHasData", "bHasData" } }   // values null when no data / NaN
// "groups" is present only when a layer registry is available. "label" is the schema enum label of the group value when it has one.
// Grouping layers: the category layers of the delta layer's grid whose schema sets `history.group` (at most
// GroupDeltaTable::kMaxGroupLayers, registry order).
//
// Cost: the group table is rebuilt once per change of its inputs, never per frame: view epoch, selection version/count, the per-cell
// window start, the registry's layers version and (only when a side is `now`) the version() of the delta layer and grouping layers.
// Hover is rebuilt when the hovered cell, the epoch or the layer version changes. BuildCount() counts table builds.
// HoverExtra (DIA_DEBUG) implements CellLayerViewerDomain::HoverExtraFn (register it with AddHoverExtraFn): in delta mode, when the hovered layer is the
// delta layer, it appends "A (<side>)", "B (<side>)" and "Delta" lines (value "%.4g", Delta "%+.4g", "no data" for a side without data / NaN delta; no units,
// same as the selected-cell detail rows) from the cached DeltaCellReader (no extra layer read per hover move). Needs a prior Update() for the registry.
// Selection restriction uses the registry's SelectionView for the delta layer's grid. Same-PU contract (the viewer PU); no singletons.

#include <DiaCellHistory/DeltaCellInfo.h>
#include <DiaCellHistory/GroupDeltaTable.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#ifdef DIA_DEBUG
#include <DiaCellLayers/CellLayerViewerDomain.h>
#endif
#include <json/json.h>

#include <cstdint>
#include <memory>

namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellHistory
{
	class CellHistoryRecorder;

	class DeltaCardState
	{
	public:
		DeltaCardState(const CellHistoryRecorder& recorder, const HistoryViewState& view);
		~DeltaCardState();
		DeltaCardState(const DeltaCardState&) = delete;
		DeltaCardState& operator=(const DeltaCardState&) = delete;

		// Optional, borrowed: where snapshot sides are read from (group table, hover). nullptr clears.
		void SetSnapshotCache(SnapshotCellCache* cache) { mBuilder.SetSnapshotCache(cache); mCells.SetSnapshotCache(cache); }

		// Once per frame. hoverQ/hoverR < 0: nothing hovered. registry and schema may be null.
		void Update(Dia::CellLayers::CellLayerRegistry* registry, const Dia::ValueSchema::ValueSchemaService* schema, int32_t hoverQ, int32_t hoverR);

#ifdef DIA_DEBUG
		void HoverExtra(const Dia::CellLayers::CellLayerViewerDomain::HoverInfo& hover, Dia::CellLayers::CellLayerViewerDomain::HoverExtraLines& out);
#endif
		const Json::Value&	Card() const { return mCard; }	// null outside delta mode
		uint32_t			BuildCount() const { return mBuilds; }

	private:
		uint64_t	LiveVersionHash(const Dia::CellLayers::CellLayerDesc& base) const;
		void		RebuildGroups(Dia::CellLayers::CellLayerRegistry& registry, const Dia::CellLayers::CellLayerDesc& base, const Dia::ValueSchema::ValueSchemaService* schema);
		void		RebuildHover(const Dia::CellLayers::CellLayerDesc* base, int32_t q, int32_t r);
		void		Clear();

		const CellHistoryRecorder&	mRecorder;
		const HistoryViewState&		mView;
		GroupDeltaBuilder			mBuilder;
		std::unique_ptr<GroupDeltaTable>	mTable;
		DeltaCellReader				mCells;
		Dia::CellLayers::CellLayerRegistry*	mRegistry = nullptr;	// last registry seen by Update (for HoverExtra)
		std::unique_ptr<Dia::CellLayers::CellLayerDesc[]>	mGroupDescs;	// kMaxGroupLayers
		uint32_t					mGroupCount = 0;
		uint8_t*					mMask = nullptr;
		uint32_t					mMaskCount = 0;

		Json::Value					mCard;
		Json::Value					mGroupsJson;
		Json::Value					mHoverJson;
		uint32_t					mBuilds = 0;

		bool						mGroupsKeyValid = false;
		uint64_t					mKeyEpoch = 0;
		uint32_t					mKeySelVersion = 0;
		uint32_t					mKeySelCount = 0;
		uint32_t					mKeyFirstRetained = 0;
		uint32_t					mKeyLayersVersion = 0;
		uint64_t					mKeyLive = 0;
		bool						mHoverKeyValid = false;
		int32_t						mKeyQ = -1, mKeyR = -1;
		uint64_t					mHoverEpoch = 0;
		uint64_t					mHoverVersion = 0;
		uint32_t					mHoverFirstRetained = 0;
	};
}
