#include <DiaCellLayers/SelectionTools.h>

namespace Dia::CellLayers::SelectionTools
{
	namespace
	{
		ToolResult Result(const CellSelection& sel, bool changed)
		{
			ToolResult res;
			res.ok = true;
			res.changed = changed;
			res.count = sel.Count();
			return res;
		}

		bool InBounds(const CellSelection& sel, int32_t q, int32_t r)
		{
			return q >= 0 && r >= 0 && uint32_t(q) < sel.Width() && uint32_t(r) < sel.Height();
		}
	}

	ToolResult SelectCell(CellSelection& sel, int32_t q, int32_t r)
	{
		if (sel.CellCount() == 0 || !InBounds(sel, q, r))
		{
			return Result(sel, false);
		}
		const uint32_t index = uint32_t(r) * sel.Width() + uint32_t(q);
		sel.BeginEdit();
		sel.Clear();
		sel.Set(index, true);
		return Result(sel, sel.EndEdit());
	}

	ToolResult ToggleCell(CellSelection& sel, int32_t q, int32_t r)
	{
		if (sel.CellCount() == 0 || !InBounds(sel, q, r))
		{
			return Result(sel, false);
		}
		const uint32_t index = uint32_t(r) * sel.Width() + uint32_t(q);
		const bool changed = sel.Set(index, !sel.Contains(index));
		return Result(sel, changed);
	}

	ToolResult SelectBox(CellSelection& sel, int32_t q0, int32_t r0, int32_t q1, int32_t r1)
	{
		if (sel.CellCount() == 0)
		{
			return Result(sel, false);
		}
		int32_t qMin = q0 < q1 ? q0 : q1;
		int32_t qMax = q0 < q1 ? q1 : q0;
		int32_t rMin = r0 < r1 ? r0 : r1;
		int32_t rMax = r0 < r1 ? r1 : r0;
		const int32_t w = int32_t(sel.Width());
		const int32_t h = int32_t(sel.Height());
		if (qMax < 0 || rMax < 0 || qMin >= w || rMin >= h)
		{
			return Result(sel, false);
		}
		if (qMin < 0) { qMin = 0; }
		if (rMin < 0) { rMin = 0; }
		if (qMax >= w) { qMax = w - 1; }
		if (rMax >= h) { rMax = h - 1; }

		sel.BeginEdit();
		sel.Clear();
		for (int32_t r = rMin; r <= rMax; ++r)
		{
			for (int32_t q = qMin; q <= qMax; ++q)
			{
				sel.Set(uint32_t(r) * uint32_t(w) + uint32_t(q), true);
			}
		}
		return Result(sel, sel.EndEdit());
	}

	ToolResult ClearSelection(CellSelection& sel)
	{
		if (sel.CellCount() == 0)
		{
			return Result(sel, false);
		}
		return Result(sel, sel.Clear());
	}
}
