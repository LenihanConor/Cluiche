////////////////////////////////////////////////////////////////////////////////
// TestHexGeometryBuilder.cpp
// Tests for the CellField HexFlatTopOddQ geometry builder (CPU only) - DiaGraphics
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaGraphics/CellField/HexFlatTopOddQBuilder.h>
#include "HexTopologyToCellGridDesc.h"

#include <cmath>

using namespace Dia::Graphics;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::CellCoord;

namespace
{
	const float kSize = 10.0f;
	const float kPi = 3.14159265358979f;

	bool BuildFor(int w, int h, CellGridGeometry& geo, HexDescStorage& storage)
	{
		HexTopology topo(w, h);
		CellGridDesc desc = HexTopologyToCellGridDesc(topo, kSize, storage);
		HexFlatTopOddQBuilder builder;
		return builder.Build(desc, geo);
	}

	void ExpectGrid(int w, int h)
	{
		HexTopology topo(w, h);
		HexDescStorage storage;
		CellGridGeometry geo;
		ASSERT_TRUE(BuildFor(w, h, geo, storage));
		const uint32_t cells = topo.CellCount();
		ASSERT_EQ(geo.vertexCount, 18u * cells);
		ASSERT_EQ(geo.indexCount, 18u * cells);

		for (uint32_t c = 0; c < cells; ++c)
		{
			const Dia::Maths::Vector2D centre = HexTopology::CellCenter(topo.ToCoord(c), kSize);
			for (uint32_t k = 0; k < 6; ++k)
			{
				const VertexCellField* v = &geo.vertices[c * 18u + k * 3u];
				const float a0 = kPi / 3.0f * k;
				const float a1 = kPi / 3.0f * (k + 1);
				for (int j = 0; j < 3; ++j)
				{
					EXPECT_NEAR(v[j].centre[0], centre.x, 1e-4f);
					EXPECT_NEAR(v[j].centre[1], centre.y, 1e-4f);
				}
				// centre vertex
				EXPECT_EQ(v[0].offset[0], 0.0f);
				EXPECT_EQ(v[0].offset[1], 0.0f);
				EXPECT_EQ(v[0].weights[0], 255);
				EXPECT_EQ(v[0].weights[1], 0);
				EXPECT_EQ(v[0].weights[2], 0);
				EXPECT_EQ(v[0].weights[3], 0);
				// corners (y-down: on-screen CCW angle theta -> (cos, -sin))
				EXPECT_NEAR(v[1].offset[0], std::cos(a0), 1e-5f);
				EXPECT_NEAR(v[1].offset[1], -std::sin(a0), 1e-5f);
				EXPECT_NEAR(v[2].offset[0], std::cos(a1), 1e-5f);
				EXPECT_NEAR(v[2].offset[1], -std::sin(a1), 1e-5f);

				// cell references [c, N(k-1), N(k), N(k+1)]
				const uint32_t dirs[3] = { (k + 5) % 6, k, (k + 1) % 6 };
				float expected[4];
				expected[0] = static_cast<float>(c);
				bool present[4] = { true, false, false, false };
				for (int j = 0; j < 3; ++j)
				{
					const uint32_t n = topo.NeighbourAt(c, dirs[j]);
					expected[1 + j] = (n == Dia::Pathfinding::kNoCell) ? -1.0f : static_cast<float>(n);
					present[1 + j] = n != Dia::Pathfinding::kNoCell;
				}
				for (int j = 0; j < 3; ++j)
					for (int m = 0; m < 4; ++m)
						EXPECT_EQ(v[j].cellIndices[m], expected[m]);

				// weights: thirds, missing -> 0, renormalised
				for (int corner = 1; corner <= 2; ++corner)
				{
					float w[4] = { 1, 0, 0, 0 };
					if (corner == 1) { w[1] = 1; w[2] = 1; } else { w[2] = 1; w[3] = 1; }
					float sum = 0;
					for (int m = 0; m < 4; ++m) { if (!present[m]) w[m] = 0; sum += w[m]; }
					int total = 0;
					for (int m = 0; m < 4; ++m)
					{
						EXPECT_NEAR(v[corner].weights[m], w[m] / sum * 255.0f, 1.01f) << "cell " << c << " k " << k << " corner " << corner;
						total += v[corner].weights[m];
					}
					EXPECT_EQ(total, 255);
					if (!present[1] && !present[2] && !present[3]) { /* nothing */ }
				}

				// winding: CCW on screen with y-down world => raw cross product negative
				const float ax = v[1].offset[0] - v[0].offset[0], ay = v[1].offset[1] - v[0].offset[1];
				const float bx = v[2].offset[0] - v[0].offset[0], by = v[2].offset[1] - v[0].offset[1];
				EXPECT_LT(ax * by - ay * bx, 0.0f);
			}
		}
		for (uint32_t i = 0; i < geo.indexCount; ++i)
			ASSERT_EQ(geo.indices[i], i);
	}
}

TEST(CellFieldGeometry, TopologyId)
{
	HexFlatTopOddQBuilder b;
	EXPECT_TRUE(b.Topology() == Dia::Core::StringCRC("hex.flat.oddq"));
}

TEST(CellFieldGeometry, VertexIs36Bytes) { EXPECT_EQ(sizeof(VertexCellField), 36u); }

TEST(CellFieldGeometry, Grid1x1) { ExpectGrid(1, 1); }
TEST(CellFieldGeometry, Grid2x2) { ExpectGrid(2, 2); }
TEST(CellFieldGeometry, Grid5x4) { ExpectGrid(5, 4); }
TEST(CellFieldGeometry, Grid66x42) { ExpectGrid(66, 42); }
TEST(CellFieldGeometry, Grid104x64) { ExpectGrid(104, 64); }

TEST(CellFieldGeometry, SingleCellCornersAreAllOwn)
{
	HexDescStorage s;
	CellGridGeometry geo;
	ASSERT_TRUE(BuildFor(1, 1, geo, s));
	for (uint32_t i = 0; i < geo.vertexCount; ++i)
	{
		EXPECT_EQ(geo.vertices[i].weights[0], 255);
		EXPECT_EQ(geo.vertices[i].cellIndices[0], 0.0f);
		EXPECT_EQ(geo.vertices[i].cellIndices[1], -1.0f);
	}
}

TEST(CellFieldGeometry, InvalidDescWritesNothing)
{
	HexDescStorage s;
	HexTopology topo(3, 3);
	const CellGridDesc good = HexTopologyToCellGridDesc(topo, kSize, s);
	HexFlatTopOddQBuilder b;

	CellGridDesc d = good; d.centresXY = nullptr;
	CellGridGeometry geo;
	EXPECT_FALSE(b.Build(d, geo));
	EXPECT_EQ(geo.vertexCount, 0u);
	EXPECT_EQ(geo.vertices, nullptr);

	d = good; d.neighbours = nullptr;
	EXPECT_FALSE(b.Build(d, geo));

	d = good; d.width = 0;
	EXPECT_FALSE(b.Build(d, geo));

	d = good; d.height = 0;
	EXPECT_FALSE(b.Build(d, geo));

	d = good; d.topology = Dia::Core::StringCRC("square.4");
	EXPECT_FALSE(b.Build(d, geo));
	EXPECT_EQ(geo.vertexCount, 0u);
	EXPECT_EQ(geo.indexCount, 0u);
	EXPECT_EQ(geo.vertices, nullptr);
	EXPECT_EQ(geo.indices, nullptr);
}
