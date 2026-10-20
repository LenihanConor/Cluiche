////////////////////////////////////////////////////////////////////////////////
// HexTopologyToCellGridDesc.h
// Test helper: copies a HexTopology into CellGridDesc plain data (the helper owns storage).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaPathfinding/HexTopology.h>

#include <cstdint>
#include <vector>

struct HexDescStorage
{
	std::vector<float>   centres;
	std::vector<int32_t> neighbours;
};

inline Dia::Graphics::CellGridDesc HexTopologyToCellGridDesc(const Dia::Pathfinding::HexTopology& topo, float size, HexDescStorage& storage)
{
	using namespace Dia::Pathfinding;
	const uint32_t count = topo.CellCount();
	storage.centres.assign(2u * count, 0.0f);
	storage.neighbours.assign(6u * count, -1);
	for (uint32_t i = 0; i < count; ++i)
	{
		const Dia::Maths::Vector2D c = HexTopology::CellCenter(topo.ToCoord(i), size);
		storage.centres[2u * i] = c.x;
		storage.centres[2u * i + 1u] = c.y;
		for (uint32_t d = 0; d < 6u; ++d)
		{
			const uint32_t n = topo.NeighbourAt(i, d);
			storage.neighbours[6u * i + d] = (n == kNoCell) ? -1 : static_cast<int32_t>(n);
		}
	}
	Dia::Graphics::CellGridDesc desc;
	desc.topology = Dia::Core::StringCRC("hex.flat.oddq");
	desc.width = static_cast<uint16_t>(topo.Width());
	desc.height = static_cast<uint16_t>(topo.Height());
	desc.cellSize = size;
	desc.centresXY = storage.centres.data();
	desc.neighbours = storage.neighbours.data();
	return desc;
}
