#include <DiaCellLayers/SelectionOutline.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaPathfinding/HexTopology.h>

namespace Dia::CellLayers
{
	namespace
	{
		// Corner j sits at on-screen CCW angle 60*j deg: (cos, -sin) in y-down world space. Edge d joins corner d and
		// d+1 and faces HexTopology direction d (normal angle 30 + 60*d).
		const float kCornerX[6] = { 1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f };
		const float kCornerY[6] = { 0.0f, -0.8660254f, -0.8660254f, 0.0f, 0.8660254f, 0.8660254f };

		struct BuildState
		{
			const CellSelection* sel;
			Dia::Pathfinding::HexTopology topo;
			OutlineEdge* edges;
			uint32_t count;
			float size;
		};

		void VisitCell(uint32_t index, void* user)
		{
			BuildState& st = *static_cast<BuildState*>(user);
			const Dia::Pathfinding::CellCoord coord = st.topo.ToCoord(index);
			const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter(coord, st.size);
			for (uint32_t d = 0; d < Dia::Pathfinding::kHexDirections; ++d)
			{
				const uint32_t n = st.topo.NeighbourAt(index, d);
				if (n != Dia::Pathfinding::kNoCell && st.sel->Contains(n))
				{
					continue;
				}
				if (st.count < SelectionOutline::kMaxEdges)
				{
					const uint32_t e = (d + 1) % 6;
					OutlineEdge& out = st.edges[st.count];
					out.x0 = c.X() + kCornerX[d] * st.size;
					out.y0 = c.Y() + kCornerY[d] * st.size;
					out.x1 = c.X() + kCornerX[e] * st.size;
					out.y1 = c.Y() + kCornerY[e] * st.size;
				}
				++st.count;		// keeps counting past the cap so overflow is detected
			}
		}
	}

	void SelectionOutline::Update(const CellSelection& sel)
	{
		if (mBuilt && mVersion == sel.Version() && mWidth == sel.Width() && mHeight == sel.Height() && mCellSize == sel.CellSize())
		{
			return;
		}
		DIA_PROFILE_SCOPE("celllayers.selection.outline", ::Dia::Observation::Profile::Category::kDiaGraphics);

		mBuilt = true;
		mVersion = sel.Version();
		mWidth = sel.Width();
		mHeight = sel.Height();
		mCellSize = sel.CellSize();
		mSelectedCount = sel.Count();
		mEdgeCount = 0;
		mTooLarge = false;
		++mRebuilds;

		if (sel.Count() == 0 || mWidth == 0 || mHeight == 0)
		{
			return;
		}

		BuildState st{ &sel, Dia::Pathfinding::HexTopology(int(mWidth), int(mHeight)), mEdges, 0, mCellSize };
		sel.ForEach(&VisitCell, &st);
		if (st.count > kMaxEdges)
		{
			mTooLarge = true;
			mEdgeCount = 0;
		}
		else
		{
			mEdgeCount = st.count;
		}
	}

	void SelectionOutline::Draw(Dia::Core::IDebugDraw& draw, Dia::Core::RGBA colour) const
	{
		if (mTooLarge)
		{
			return;
		}
		for (uint32_t i = 0; i < mEdgeCount; ++i)
		{
			const OutlineEdge& e = mEdges[i];
			draw.RequestDraw(Dia::Maths::Vector2D(e.x0, e.y0), Dia::Maths::Vector2D(e.x1, e.y1), colour);
		}
	}
}
