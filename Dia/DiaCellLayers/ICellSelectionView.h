#pragma once

// ICellSelectionView: read-only view of the cell selection on one grid. Handed to systems (and to overlay draw callbacks)
// by the CellLayerRegistry (SelectionView(grid)); e.g. a paint system reads it to apply a value to the selected cells.
// Cell identity is the grid index (row-major: r * width + q). Version() changes on every change to the set and not
// otherwise. The pointer is borrowed from the registry. Generic: no game types (CL-005), no singletons (CL-006).

#include <stdint.h>

namespace Dia::CellLayers
{
	class ICellSelectionView
	{
	public:
		virtual ~ICellSelectionView() {}

		virtual uint32_t	Version() const = 0;
		virtual uint32_t	Count() const = 0;
		virtual bool		Contains(uint32_t cellIndex) const = 0;
		// Visits selected cells in ascending index order.
		virtual void		ForEach(void (*fn)(uint32_t cellIndex, void* user), void* user) const = 0;
	};
}
