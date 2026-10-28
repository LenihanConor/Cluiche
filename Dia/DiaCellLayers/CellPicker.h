#pragma once

// CellPicker: screen point -> cell. Inverse of the CellField/debug ortho camera transform (Dia::Bgfx::BuildOrtho2D:
// world +Y points DOWN the screen, pixel origin top-left, rotation not applied, zoom <= 0 treated as 1), then
// HexTopology::CellFromPoint using the grid's cellSize. An out-of-bounds, non-finite or absurd result returns no cell.
// Read-only and stateless (Feature 2 selection reuses it); takes plain data so it needs no service access.
// Generic: no game types (CL-005), no singletons (CL-006).
//
// The viewer builds a CellPickerView each frame from its Camera2D (position, zoom) and the canvas size in pixels,
// and reads the cursor from the panel/game input in the same pixel space as the canvas.

#include <DiaGraphics/CellField/CellGridDesc.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	struct CellPickerView
	{
		float viewportWidth = 0.0f;		// canvas size in pixels
		float viewportHeight = 0.0f;
		float cameraX = 0.0f;			// world point shown at the canvas centre
		float cameraY = 0.0f;
		float zoom = 1.0f;				// > 1 zoomed in; <= 0 treated as 1
	};

	struct PickedCell
	{
		bool		valid = false;
		int32_t		q = -1;					// column
		int32_t		r = -1;					// row
		uint32_t	index = 0xFFFFFFFFu;	// row-major: r * width + q
	};

	class CellPicker
	{
	public:
		// Pixel (top-left origin, Y down) -> world, the inverse of BuildOrtho2D + MatrixWorldToPixel.
		static void			ScreenToWorld(const CellPickerView& view, float screenX, float screenY, float& worldX, float& worldY);
		static PickedCell	Pick(const CellPickerView& view, float screenX, float screenY, const Dia::Graphics::CellGridDesc& grid);
	};
}
