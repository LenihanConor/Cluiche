#pragma once

// Test helpers for CellPicker (Testing/ convention): forward projection that mirrors Dia::Bgfx::WorldToPixel
// (BuildOrtho2D + MatrixWorldToPixel) without the bgfx dependency.

#include <DiaCellLayers/CellPicker.h>

namespace Dia::CellLayers::Testing
{
	inline void WorldToScreen(const CellPickerView& view, float wx, float wy, float& sx, float& sy)
	{
		const float zoom = view.zoom > 0.0f ? view.zoom : 1.0f;
		sx = (wx - view.cameraX) * zoom + view.viewportWidth * 0.5f;
		sy = (wy - view.cameraY) * zoom + view.viewportHeight * 0.5f;
	}
}
