#pragma once

// Test helpers for CellSelection (Testing/ convention): grid builder and selection -> vector.

#include <DiaCellLayers/CellSelection.h>

#include <vector>

namespace Dia::CellLayers::Testing
{
	// Plain grid description with only width/height/cellSize set (no centres/neighbours).
	inline Dia::Graphics::CellGridDesc MakeGrid(uint16_t w, uint16_t h, float cellSize = 1.0f)
	{
		Dia::Graphics::CellGridDesc g;
		g.width = w;
		g.height = h;
		g.cellSize = cellSize;
		return g;
	}

	// Selected cell indices in ascending order.
	inline std::vector<uint32_t> ToVector(const CellSelection& s)
	{
		std::vector<uint32_t> out;
		s.ForEach([](uint32_t i, void* u) { static_cast<std::vector<uint32_t>*>(u)->push_back(i); }, &out);
		return out;
	}
}
