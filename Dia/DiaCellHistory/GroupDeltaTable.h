#pragma once

// GroupDeltaTable: the inspector's group summary for delta mode (Feature 4). For the delta's base layer it answers, per group of
// each GROUPING layer (a category layer; the value is the enum index), "what was the mean on side A, on side B, and the change".
//
// Decisions (binding for callers and tests):
//  - Cells used. A cell contributes to a group only when BOTH base values (A and B) are non-NaN, so meanA, meanB and change
//    describe the same cell set. `count` is the number of cells used.
//  - Group membership. A cell's group is its grouping-layer value, which must be a whole number in [0, kGroupValueCap). NaN,
//    negative, fractional and >= kGroupValueCap values place the cell in no group (it is ignored for that grouping layer).
//  - Which side groups. Membership comes from the grouping layer's values on the chosen side (DeltaSpec::groupSideB: true = B,
//    false = A). When that side has no data for the grouping layer (SideReader NoData / Unavailable), the LIVE read of the
//    grouping layer is used instead and usedLive[groupLayerIndex] is set (the card says so); a grouping layer without a live
//    read is skipped. A static grouping layer yields the same table for either side. The BASE layer never falls back to live
//    (a `now` side legitimately reads live): if the base has no data on a side the table is empty and baseNoData is set.
//  - Selection. `selectedMask` is a plain byte per cell (non-zero = selected); nullptr = no selection (all cells). A group with no
//    cells in the restriction is omitted; an all-zero mask yields no rows. Plain mask chosen over the viewer's selection type to
//    keep this module free of viewer internals; the caller converts.
//  - change = meanB - meanA. Rows are sorted by |change| descending; ties break by smaller groupValue, then smaller
//    groupLayerIndex (grouping layer order). Rows are capped at kMaxRows: when more qualify the largest-|change| rows (by that same
//    ordering) are kept and `truncated` is set.
//  - At most kMaxGroupLayers grouping layers are processed; later ones are ignored.
// Data only (no JSON here). Same-PU contract as the recorder; no singletons. The builder allocates three scratch buffers of
// CellCount floats on first use and reuses them; Build performs no other heap allocation. Profile scope cellhistory.group_table.
// One HIS Warning is logged per builder when a grouping layer falls back to live (re-armed by ResetWarnings()).

#include <DiaCellHistory/SideReader.h>

#include <cstdint>

namespace Dia::CellLayers { struct CellLayerDesc; }

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	struct DeltaSpec;

	struct GroupDeltaRow
	{
		uint32_t	groupLayerIndex = 0;	// index into the grouping layer list passed to Build
		uint32_t	groupValue = 0;			// enum index of the group
		uint32_t	count = 0;				// cells used (both base sides non-NaN, in the selection)
		double		meanA = 0.0;
		double		meanB = 0.0;
		double		change = 0.0;			// meanB - meanA
	};

	struct GroupDeltaTable
	{
		static constexpr uint32_t kMaxRows = 512;
		static constexpr uint32_t kMaxGroupLayers = 16;
		static constexpr uint32_t kGroupValueCap = 256;

		GroupDeltaRow	rows[kMaxRows];
		uint32_t		rowCount = 0;
		bool			truncated = false;
		bool			baseNoData = false;
		bool			usedLive[kMaxGroupLayers] = {};	// by groupLayerIndex: membership came from live values (chosen side had none)
		bool			skipped[kMaxGroupLayers] = {};	// by groupLayerIndex: no data on the chosen side and no live read

		void Clear();
	};

	// True when `v` is a valid group value: whole number in [0, kGroupValueCap).
	bool IsGroupValue(float v);

	// Accumulates one grouping layer into `table` (appends rows; keeps the largest when over kMaxRows). n cells; a, b are the base
	// values per side, groupValues the grouping layer; selectedMask may be nullptr. Does not sort.
	void AccumulateGroups(GroupDeltaTable& table, uint32_t groupLayerIndex, const float* a, const float* b, const float* groupValues, uint32_t n, const uint8_t* selectedMask);

	// Sorts rows by |change| descending, ties by groupValue then groupLayerIndex ascending.
	void SortByAbsChange(GroupDeltaTable& table);

	class GroupDeltaBuilder
	{
	public:
		explicit GroupDeltaBuilder(const CellHistoryRecorder& recorder);
		~GroupDeltaBuilder();
		GroupDeltaBuilder(const GroupDeltaBuilder&) = delete;
		GroupDeltaBuilder& operator=(const GroupDeltaBuilder&) = delete;

		// Builds the table for `base` under `delta` (sides a/b, groupSideB). selectedMask is CellCount bytes or nullptr.
		void Build(const Dia::CellLayers::CellLayerDesc& base, const DeltaSpec& delta, const Dia::CellLayers::CellLayerDesc* groupLayers,
			uint32_t groupLayerCount, const uint8_t* selectedMask, GroupDeltaTable& out);

		// Re-arms the once-per-builder live-fallback warning (call on a new delta).
		void ResetWarnings() { mWarned = false; }

	private:
		const CellHistoryRecorder&	mRecorder;
		SideReader					mReader;
		float*						mBufA = nullptr;
		float*						mBufB = nullptr;
		float*						mBufG = nullptr;
		uint32_t					mBufCount = 0;
		bool						mWarned = false;
	};
}
