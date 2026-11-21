#include <DiaCellLayers/CellPicker.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaPathfinding/HexTopology.h>

#include <math.h>

namespace Dia::CellLayers
{
	namespace
	{
		const float kMaxWorld = 1.0e7f;	// beyond this the int cast in CellFromPoint would be unsafe
	}

	void CellPicker::ScreenToWorld(const CellPickerView& view, float screenX, float screenY, float& worldX, float& worldY)
	{
		const float zoom = view.zoom > 0.0f ? view.zoom : 1.0f;
		worldX = view.cameraX + (screenX - view.viewportWidth * 0.5f) / zoom;
		worldY = view.cameraY + (screenY - view.viewportHeight * 0.5f) / zoom;
	}

	PickedCell CellPicker::Pick(const CellPickerView& view, float screenX, float screenY, const Dia::Graphics::CellGridDesc& grid)
	{
		PickedCell out;
		if (grid.width == 0 || grid.height == 0 || !(grid.cellSize > 0.0f)) { return out; }

		float wx = 0.0f, wy = 0.0f;
		ScreenToWorld(view, screenX, screenY, wx, wy);
		if (!isfinite(wx) || !isfinite(wy) || fabsf(wx) > kMaxWorld || fabsf(wy) > kMaxWorld) { return out; }

		const Dia::Pathfinding::HexTopology topology(grid.width, grid.height);
		const Dia::Pathfinding::CellCoord c = Dia::Pathfinding::HexTopology::CellFromPoint(Dia::Maths::Vector2D(wx, wy), grid.cellSize);
		if (!topology.IsInBounds(c)) { return out; }

		out.valid = true;
		out.q = c.x;
		out.r = c.y;
		out.index = topology.ToIndex(c);
		return out;
	}
}
