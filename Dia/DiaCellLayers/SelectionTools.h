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
}
