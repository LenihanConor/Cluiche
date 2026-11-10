////////////////////////////////////////////////////////////////////////////////
// Filename: HexFlatTopOddQBuilder.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/CellField/ICellGridGeometryBuilder.h>

namespace Dia
{
	namespace Graphics
	{
		///
		/// Geometry builder for flat-top odd-q hex grids (topology id "hex.flat.oddq").
		///
		/// Conventions (match Dia::Pathfinding::HexTopology):
		///  - World space is y-DOWN. Corner k sits at on-screen counter-clockwise angle 60deg*k,
		///    so its offset is (cos, -sin) of that angle.
		///  - Neighbour direction d (desc.neighbours[6*cell + d]) is at on-screen angle 30deg+60deg*d,
		///    i.e. direction k is the edge between corner k and corner k+1.
		///  - Per hex: 6 triangles (centre, corner k, corner k+1), k = 0..5, 18 vertices, 18 indices
		///    (sequential, no shared vertices). Each triangle is counter-clockwise ON SCREEN, which in
		///    the raw y-down coordinates is a NEGATIVE cross product ((b-a) x (c-a) < 0).
		///  - Cell references [own, N(k-1), N(k), N(k+1)]; weights: centre (1,0,0,0), corner k
		///    (1/3,1/3,1/3,0), corner k+1 (1/3,0,1/3,1/3); missing neighbours get 0 and the rest are
		///    renormalised. Weights are unorm8 summing to exactly 255 (largest absorbs rounding).
		///
		class HexFlatTopOddQBuilder : public ICellGridGeometryBuilder
		{
		public:
			Dia::Core::StringCRC Topology() const override;
			bool Build(const CellGridDesc& desc, CellGridGeometry& out) const override;
		};
	}
}
