////////////////////////////////////////////////////////////////////////////////
// Filename: CellGridDesc.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>

#include <stdint.h>

namespace Dia
{
	namespace Graphics
	{
		///
		/// CellGridDesc - Plain-data description of a cell grid, copied by the caller from its
		/// topology so DiaGraphics needs no topology type. Pointers are borrowed (not owned).
		///
		struct CellGridDesc
		{
			Dia::Core::StringCRC id;					// optional stable grid id, e.g. "world.hex"; empty = unnamed
			Dia::Core::StringCRC topology;				// e.g. "hex.flat.oddq"
			uint16_t             width = 0;
			uint16_t             height = 0;
			float                cellSize = 0.0f;		// centre-to-corner
			const float*         centresXY = nullptr;	// 2 * width*height (x, y per cell)
			const int32_t*       neighbours = nullptr;	// 6 * width*height, -1 = none
		};
	}
}
