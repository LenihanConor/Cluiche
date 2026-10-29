#pragma once

// SelectionTools: pure tool functions over a CellSelection (the DiaAPI command wiring lives elsewhere).
// Coordinates are cell (q = column, r = row); cell index = r * width + q. Out-of-bounds cells are ignored
// (ok = true, changed = false). Each call bumps the selection version at most once, and only on net change.
// A selection with no grid is a no-op. Generic: no game types (CL-005), no singletons (CL-006).

#include <DiaCellLayers/CellSelection.h>

#include <stdint.h>

namespace Dia::CellLayers::SelectionTools
{
	struct ToolResult
	{
		bool		ok = false;			// arguments were valid
		bool		changed = false;	// selection set changed
		uint32_t	count = 0;			// selection size after the call
	};

	ToolResult SelectCell(CellSelection& sel, int32_t q, int32_t r);		// replace with that cell
	ToolResult ToggleCell(CellSelection& sel, int32_t q, int32_t r);		// add if absent, remove if present
	ToolResult SelectBox(CellSelection& sel, int32_t q0, int32_t r0, int32_t q1, int32_t r1);	// inclusive rect, any corner order, clamped
	ToolResult ClearSelection(CellSelection& sel);

		// A polygon vertex in WORLD space: the same space as HexTopology::CellCenter(q, r, cellSize) (flat-top odd-q,
		// y down), with the grid's cellSize taken from CellSelection::CellSize(). Pass CellPicker::ScreenToWorld output.
		struct PolyPoint
		{
			float x;
			float y;
		};

		// Replace the selection with the cells whose centres lie inside the polygon (even-odd rule, so concave and
		// self-overlapping polygons work). Needs >= 3 vertices, all finite, and a grid with cellSize > 0; otherwise
		// ok = false and the selection is unchanged. The polygon is implicitly closed. Edge rule (half-open ray
		// crossing): a centre exactly on a minimum-x or minimum-y edge is inside; on a maximum-x or maximum-y edge it
		// is outside. A polygon that misses the grid is valid and yields an empty selection (it IS a replace).
		ToolResult SelectPolygon(CellSelection& sel, const PolyPoint* vertices, uint32_t vertexCount);

		constexpr uint32_t kMaxCellsPerCall = 2048;

		struct CellsResult : ToolResult
		{
			uint32_t	requested = 0;		// pairCount as given
			uint32_t	accepted = 0;		// distinct in-bounds cells among the first kMaxCellsPerCall pairs
			bool		overCap = false;	// pairCount exceeded kMaxCellsPerCall (extras ignored; caller logs the Warning)
		};

		// Replace the selection with exactly the listed cells. qr is interleaved q,r pairs (2 * pairCount ints).
		// Out-of-bounds and duplicate pairs are ignored. An empty or all-invalid list yields an empty selection.
		// If acceptedQR is non-null, the accepted pairs (input order, deduplicated) are written to it, at most
		// acceptedCapacity pairs; `accepted` still counts all of them.
		CellsResult SelectCells(CellSelection& sel, const int32_t* qr, uint32_t pairCount, uint32_t* acceptedQR = nullptr, uint32_t acceptedCapacity = 0);
}
