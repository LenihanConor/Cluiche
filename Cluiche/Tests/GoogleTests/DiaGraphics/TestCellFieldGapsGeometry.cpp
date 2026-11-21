////////////////////////////////////////////////////////////////////////////////
// TestCellFieldGapsGeometry.cpp
// Task 26 gap-audit tests: HexFlatTopOddQBuilder + CellGridGeometry - DiaGraphics
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaGraphics/CellField/HexFlatTopOddQBuilder.h>
#include "HexTopologyToCellGridDesc.h"

#include <cmath>
#include <limits>
#include <string.h>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Dia::Graphics;
using Dia::Pathfinding::HexTopology;

namespace
{
	const float kSize = 10.0f;

	// Checks every structural invariant of a built grid against the real HexTopology.
	// Returns the number of failing checks (so large sweeps stay readable).
	int CheckGrid(int w, int h)
	{
		int failures = 0;
		HexTopology topo(w, h);
		HexDescStorage st;
		const CellGridDesc desc = HexTopologyToCellGridDesc(topo, kSize, st);
		CellGridGeometry geo;
		HexFlatTopOddQBuilder b;
		if (!b.Build(desc, geo)) { return 1; }
		const uint32_t cells = topo.CellCount();
		if (geo.vertexCount != 18u * cells || geo.indexCount != 18u * cells) { ++failures; }

		for (uint32_t i = 0; i < geo.indexCount; ++i)
		{
			if (geo.indices[i] != i) { ++failures; break; }
		}
		for (uint32_t i = 0; i < geo.vertexCount; ++i)
		{
			const VertexCellField& v = geo.vertices[i];
			const uint32_t cell = i / 18u;
			int sum = v.weights[0] + v.weights[1] + v.weights[2] + v.weights[3];
			if (sum != 255) { ++failures; }
			if (!std::isfinite(v.centre[0]) || !std::isfinite(v.centre[1]) || !std::isfinite(v.offset[0]) || !std::isfinite(v.offset[1])) { ++failures; }
			for (int m = 0; m < 4; ++m)
			{
				if (!std::isfinite(v.cellIndices[m])) { ++failures; }
				// absent reference <=> zero weight; present reference is a real cell
				const bool absent = v.cellIndices[m] < 0.0f;
				if (absent && (v.weights[m] != 0 || v.cellIndices[m] != -1.0f)) { ++failures; }
				if (!absent && v.cellIndices[m] >= (float)cells) { ++failures; }
			}
			if (v.cellIndices[0] != (float)cell) { ++failures; }
			if (v.weights[0] == 0) { ++failures; }	// own cell always contributes
		}
		return failures;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Grid shapes against the real HexTopology
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsGeometry, DegenerateAndParityShapesSatisfyInvariants)
{
	const int shapes[][2] = { {1,1}, {1,2}, {1,3}, {1,7}, {2,1}, {3,1}, {7,1}, {2,2}, {2,3}, {3,2}, {3,3}, {4,4}, {5,4}, {4,5}, {5,5}, {6,7}, {7,6} };
	for (const auto& s : shapes)
	{
		EXPECT_EQ(0, CheckGrid(s[0], s[1])) << s[0] << "x" << s[1];
	}
}

TEST(CellFieldGapsGeometry, EverySizeUpTo12x12SatisfiesInvariants)
{
	for (int w = 1; w <= 12; ++w)
		for (int h = 1; h <= 12; ++h)
			ASSERT_EQ(0, CheckGrid(w, h)) << w << "x" << h;
}

TEST(CellFieldGapsGeometry, MaxDimensionGridsSatisfyInvariants)
{
	EXPECT_EQ(0, CheckGrid(128, 1));
	EXPECT_EQ(0, CheckGrid(1, 128));
	EXPECT_EQ(0, CheckGrid(127, 128));
	EXPECT_EQ(0, CheckGrid(128, 127));
	EXPECT_EQ(0, CheckGrid(128, 128));
}

TEST(CellFieldGapsGeometry, Grid128x128VertexCountExceedsUint16AndIndicesReachMax)
{
	HexTopology topo(128, 128);
	HexDescStorage st;
	const CellGridDesc desc = HexTopologyToCellGridDesc(topo, kSize, st);
	CellGridGeometry geo;
	HexFlatTopOddQBuilder b;
	ASSERT_TRUE(b.Build(desc, geo));
	EXPECT_EQ(294912u, geo.vertexCount);
	EXPECT_GT(geo.vertexCount, 65536u);
	EXPECT_EQ(294911u, geo.indices[geo.indexCount - 1]);
}

TEST(CellFieldGapsGeometry, NeighbourTablesMatchTopologyAtCornersAndEdges)
{
	// Exact cell references for every vertex (not just weights), including all six directions
	// at the four corners and along the edges of odd/even-sized grids.
	const int shapes[][2] = { {2,2}, {3,3}, {4,5}, {5,4}, {1,6}, {6,1} };
	for (const auto& s : shapes)
	{
		HexTopology topo(s[0], s[1]);
		HexDescStorage st;
		const CellGridDesc desc = HexTopologyToCellGridDesc(topo, kSize, st);
		CellGridGeometry geo;
		HexFlatTopOddQBuilder b;
		ASSERT_TRUE(b.Build(desc, geo));
		for (uint32_t c = 0; c < topo.CellCount(); ++c)
		{
			for (uint32_t k = 0; k < 6; ++k)
			{
				const uint32_t dirs[3] = { (k + 5) % 6, k, (k + 1) % 6 };
				for (int j = 0; j < 3; ++j)
				{
					const VertexCellField& v = geo.vertices[c * 18u + k * 3u + j];
					EXPECT_EQ((float)c, v.cellIndices[0]);
					for (int m = 0; m < 3; ++m)
					{
						const uint32_t n = topo.NeighbourAt(c, dirs[m]);
						const float expect = (n == Dia::Pathfinding::kNoCell) ? -1.0f : (float)n;
						EXPECT_EQ(expect, v.cellIndices[1 + m]) << s[0] << "x" << s[1] << " cell " << c << " k " << k;
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Exhaustive neighbour-presence masks: every subset of 6 neighbours, every corner
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsGeometry, AllSixtyFourNeighbourMasksRenormaliseToExactly255)
{
	for (uint32_t mask = 0; mask < 64; ++mask)
	{
		// 7 cells (width 7, height 1): cell 0 has neighbour d = (d+1) when bit d set, else -1.
		std::vector<float> centres(14, 0.0f);
		std::vector<int32_t> nb(42, -1);
		for (uint32_t d = 0; d < 6; ++d)
			if (mask & (1u << d)) { nb[d] = (int32_t)(d + 1); }
		CellGridDesc desc;
		desc.topology = Dia::Core::StringCRC("hex.flat.oddq");
		desc.width = 7;
		desc.height = 1;
		desc.cellSize = 1.0f;
		desc.centresXY = centres.data();
		desc.neighbours = nb.data();
		CellGridGeometry geo;
		HexFlatTopOddQBuilder b;
		ASSERT_TRUE(b.Build(desc, geo)) << mask;

		for (uint32_t k = 0; k < 6; ++k)
		{
			const uint32_t dirs[3] = { (k + 5) % 6, k, (k + 1) % 6 };
			bool present[4] = { true, (mask >> dirs[0] & 1) != 0, (mask >> dirs[1] & 1) != 0, (mask >> dirs[2] & 1) != 0 };
			int presentCount = 0;
			for (int m = 0; m < 4; ++m) { presentCount += present[m] ? 1 : 0; }

			for (int j = 0; j < 3; ++j)
			{
				const VertexCellField& v = geo.vertices[k * 3u + j];
				int sum = 0, nonZero = 0;
				for (int m = 0; m < 4; ++m)
				{
					sum += v.weights[m];
					if (v.weights[m] != 0) { ++nonZero; }
					if (!present[m]) { EXPECT_EQ(0, v.weights[m]) << "mask " << mask << " k " << k << " j " << j << " m " << m; }
					EXPECT_EQ(present[m], v.cellIndices[m] >= 0.0f);
				}
				EXPECT_EQ(255, sum) << "mask " << mask << " k " << k << " j " << j;
				if (j == 0)
				{
					EXPECT_EQ(255, v.weights[0]);
				}
				else
				{
					// corner weights are over the present members of its triple
					const int tripleCount = j == 1 ? (1 + (int)present[1] + (int)present[2]) : (1 + (int)present[2] + (int)present[3]);
					EXPECT_EQ(tripleCount, nonZero) << "mask " << mask << " k " << k << " j " << j;
					// no weight deviates from an equal split by more than one unorm step
					for (int m = 0; m < 4; ++m)
					{
						if (v.weights[m] != 0) { EXPECT_NEAR(v.weights[m], 255.0f / tripleCount, 1.01f); }
					}
				}
			}
			(void)presentCount;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Determinism, validation, untouched-on-failure
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsGeometry, BuildIsByteIdenticalAcrossRuns)
{
	HexTopology topo(9, 8);
	HexDescStorage st;
	const CellGridDesc desc = HexTopologyToCellGridDesc(topo, kSize, st);
	HexFlatTopOddQBuilder b;
	CellGridGeometry a, c;
	ASSERT_TRUE(b.Build(desc, a));
	ASSERT_TRUE(b.Build(desc, c));
	ASSERT_EQ(a.vertexCount, c.vertexCount);
	EXPECT_EQ(0, memcmp(a.vertices, c.vertices, a.vertexCount * sizeof(VertexCellField)));
	EXPECT_EQ(0, memcmp(a.indices, c.indices, a.indexCount * sizeof(uint32_t)));

	// A fresh builder instance gives the same bytes too.
	HexFlatTopOddQBuilder b2;
	CellGridGeometry d;
	ASSERT_TRUE(b2.Build(desc, d));
	EXPECT_EQ(0, memcmp(a.vertices, d.vertices, a.vertexCount * sizeof(VertexCellField)));
}

TEST(CellFieldGapsGeometry, RebuildIntoSameOutReplacesContent)
{
	HexFlatTopOddQBuilder b;
	HexDescStorage s1, s2;
	HexTopology t1(2, 2), t2(3, 3);
	CellGridGeometry geo;
	ASSERT_TRUE(b.Build(HexTopologyToCellGridDesc(t1, kSize, s1), geo));
	EXPECT_EQ(72u, geo.vertexCount);
	ASSERT_TRUE(b.Build(HexTopologyToCellGridDesc(t2, kSize, s2), geo));
	EXPECT_EQ(162u, geo.vertexCount);
	EXPECT_EQ(162u, geo.indexCount);
}

TEST(CellFieldGapsGeometry, BadNeighbourIndexRejectedAndOutUntouched)
{
	HexTopology topo(3, 3);
	HexDescStorage st;
	const CellGridDesc good = HexTopologyToCellGridDesc(topo, kSize, st);
	HexFlatTopOddQBuilder b;

	// Pre-populate 'out' so "untouched" is observable.
	HexDescStorage st2;
	HexTopology small(2, 1);
	CellGridGeometry out;
	ASSERT_TRUE(b.Build(HexTopologyToCellGridDesc(small, kSize, st2), out));
	VertexCellField* const vPtr = out.vertices;
	uint32_t* const iPtr = out.indices;
	const uint32_t vCount = out.vertexCount;
	std::vector<VertexCellField> snapshot(out.vertices, out.vertices + vCount);

	const int32_t bad[] = { 9, 10, 1000, -2, -100, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() };
	for (int32_t v : bad)
	{
		std::vector<int32_t> nb = st.neighbours;
		nb[17] = v;
		CellGridDesc d = good;
		d.neighbours = nb.data();
		EXPECT_FALSE(b.Build(d, out)) << v;
		EXPECT_EQ(vPtr, out.vertices);
		EXPECT_EQ(iPtr, out.indices);
		EXPECT_EQ(vCount, out.vertexCount);
		EXPECT_EQ(0, memcmp(snapshot.data(), out.vertices, vCount * sizeof(VertexCellField)));
	}
	// Last valid cell index (cells-1) and -1 are accepted.
	std::vector<int32_t> nb = st.neighbours;
	nb[17] = 8;
	nb[16] = -1;
	CellGridDesc d = good;
	d.neighbours = nb.data();
	EXPECT_TRUE(b.Build(d, out));
}

TEST(CellFieldGapsGeometry, NonReciprocalAndSelfNeighboursAreAcceptedAsData)
{
	// The builder validates ranges only; reciprocity is the caller's contract (documented behaviour).
	std::vector<float> centres(4, 0.0f);
	std::vector<int32_t> nb(12, -1);
	nb[0] = 1;	// cell 0 -> 1, but 1 does not point back
	nb[6 + 2] = 1;	// cell 1 -> itself
	CellGridDesc desc;
	desc.topology = Dia::Core::StringCRC("hex.flat.oddq");
	desc.width = 2;
	desc.height = 1;
	desc.cellSize = 1.0f;
	desc.centresXY = centres.data();
	desc.neighbours = nb.data();
	CellGridGeometry geo;
	HexFlatTopOddQBuilder b;
	ASSERT_TRUE(b.Build(desc, geo));
	EXPECT_EQ(36u, geo.vertexCount);
	for (uint32_t i = 0; i < geo.vertexCount; ++i)
	{
		int sum = 0;
		for (int m = 0; m < 4; ++m) { sum += geo.vertices[i].weights[m]; }
		EXPECT_EQ(255, sum);
	}
}

TEST(CellFieldGapsGeometry, NonFiniteCentresPassThroughVerbatimWithoutAffectingWeights)
{
	HexTopology topo(2, 2);
	HexDescStorage st;
	CellGridDesc desc = HexTopologyToCellGridDesc(topo, kSize, st);
	st.centres[0] = std::numeric_limits<float>::quiet_NaN();
	st.centres[3] = std::numeric_limits<float>::infinity();
	CellGridGeometry geo;
	HexFlatTopOddQBuilder b;
	ASSERT_TRUE(b.Build(desc, geo));
	EXPECT_TRUE(std::isnan(geo.vertices[0].centre[0]));
	EXPECT_TRUE(std::isinf(geo.vertices[18].centre[1]));
	for (uint32_t i = 0; i < geo.vertexCount; ++i)
	{
		int sum = 0;
		for (int m = 0; m < 4; ++m) { sum += geo.vertices[i].weights[m]; }
		EXPECT_EQ(255, sum);
		EXPECT_TRUE(std::isfinite(geo.vertices[i].offset[0]));
	}
}

TEST(CellFieldGapsGeometry, BuilderAcceptsOutOfRegistryGridSizes)
{
	// The 128 limit belongs to the registry, not the builder.
	std::vector<float> centres(2 * 200, 0.0f);
	std::vector<int32_t> nb(6 * 200, -1);
	CellGridDesc desc;
	desc.topology = Dia::Core::StringCRC("hex.flat.oddq");
	desc.width = 200;
	desc.height = 1;
	desc.cellSize = 1.0f;
	desc.centresXY = centres.data();
	desc.neighbours = nb.data();
	CellGridGeometry geo;
	HexFlatTopOddQBuilder b;
	EXPECT_TRUE(b.Build(desc, geo));
	EXPECT_EQ(200u * 18u, geo.vertexCount);
}

////////////////////////////////////////////////////////////////////////////////
// CellGridGeometry ownership
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsGeometry, GeometryDefaultIsEmpty)
{
	CellGridGeometry g;
	EXPECT_EQ(0u, g.vertexCount);
	EXPECT_EQ(0u, g.indexCount);
	EXPECT_EQ(nullptr, g.vertices);
	EXPECT_EQ(nullptr, g.indices);
}

TEST(CellFieldGapsGeometry, GeometryAllocateAndClear)
{
	CellGridGeometry g;
	g.Allocate(6, 9);
	EXPECT_EQ(6u, g.vertexCount);
	EXPECT_EQ(9u, g.indexCount);
	ASSERT_NE(nullptr, g.vertices);
	ASSERT_NE(nullptr, g.indices);
	g.indices[8] = 77;
	EXPECT_EQ(77u, g.indices[8]);

	g.Allocate(3, 3);	// reallocation replaces
	EXPECT_EQ(3u, g.vertexCount);
	EXPECT_EQ(3u, g.indexCount);

	g.Clear();
	EXPECT_EQ(0u, g.vertexCount);
	EXPECT_EQ(0u, g.indexCount);
	EXPECT_EQ(nullptr, g.vertices);
	EXPECT_EQ(nullptr, g.indices);
	g.Clear();	// idempotent
	EXPECT_EQ(nullptr, g.vertices);
}

TEST(CellFieldGapsGeometry, GeometryMoveConstructTransfersAndEmptiesSource)
{
	CellGridGeometry a;
	a.Allocate(4, 5);
	VertexCellField* const vp = a.vertices;
	uint32_t* const ip = a.indices;
	CellGridGeometry b(std::move(a));
	EXPECT_EQ(vp, b.vertices);
	EXPECT_EQ(ip, b.indices);
	EXPECT_EQ(4u, b.vertexCount);
	EXPECT_EQ(5u, b.indexCount);
	EXPECT_EQ(nullptr, a.vertices);
	EXPECT_EQ(nullptr, a.indices);
	EXPECT_EQ(0u, a.vertexCount);
	EXPECT_EQ(0u, a.indexCount);
}

TEST(CellFieldGapsGeometry, GeometryMoveAssignTransfersAndEmptiesSource)
{
	CellGridGeometry a, b;
	a.Allocate(4, 5);
	b.Allocate(10, 10);
	VertexCellField* const vp = a.vertices;
	b = std::move(a);
	EXPECT_EQ(vp, b.vertices);
	EXPECT_EQ(4u, b.vertexCount);
	EXPECT_EQ(5u, b.indexCount);
	EXPECT_EQ(nullptr, a.vertices);
	EXPECT_EQ(nullptr, a.indices);
	EXPECT_EQ(0u, a.vertexCount);
	EXPECT_EQ(0u, a.indexCount);
}

TEST(CellFieldGapsGeometry, GeometryIsNotCopyable)
{
	EXPECT_FALSE(std::is_copy_constructible<CellGridGeometry>::value);
	EXPECT_FALSE(std::is_copy_assignable<CellGridGeometry>::value);
	EXPECT_TRUE(std::is_move_constructible<CellGridGeometry>::value);
}
