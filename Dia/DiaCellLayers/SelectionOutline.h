#pragma once

// SelectionOutline: perimeter edges of a CellSelection (flat-top odd-q hex), cached per selection version.
// Only sides between a selected cell and a non-selected/missing neighbour are kept. Above kMaxEdges the outline is
// skipped (TooLarge) and nothing is stored or drawn; the selection itself is unaffected.
// Edge i of a cell connects corner i and i+1 and faces HexTopology direction i (corner j at CCW angle 60*j deg).
// Generic: no game types (CL-005), no singletons (CL-006), no STL (PD-004).

#include <DiaCellLayers/CellSelection.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	struct OutlineEdge
	{
		float x0 = 0.0f;
		float y0 = 0.0f;
		float x1 = 0.0f;
		float y1 = 0.0f;
	};

	class SelectionOutline
	{
	public:
		static const uint32_t kMaxEdges = 4000;

		static Dia::Core::RGBA DefaultColour() { return Dia::Core::RGBA(255, 230, 0, 255); }

		// Rebuilds only when the selection version or identity (width, height, cellSize) changed.
		void				Update(const CellSelection& sel);

		bool				TooLarge() const { return mTooLarge; }
		uint32_t			EdgeCount() const { return mEdgeCount; }
		uint32_t			SelectedCountForBanner() const { return mSelectedCount; }
		uint32_t			Rebuilds() const { return mRebuilds; }
		const OutlineEdge*	Edges() const { return mEdges; }

		// One line primitive per stored edge. Nothing when too large or empty.
		void				Draw(Dia::Core::IDebugDraw& draw, Dia::Core::RGBA colour = DefaultColour()) const;

	private:
		OutlineEdge	mEdges[kMaxEdges];
		uint32_t	mEdgeCount = 0;
		uint32_t	mSelectedCount = 0;
		uint32_t	mRebuilds = 0;
		uint32_t	mVersion = 0;
		uint32_t	mWidth = 0;
		uint32_t	mHeight = 0;
		float		mCellSize = 0.0f;
		bool		mBuilt = false;
		bool		mTooLarge = false;
	};
}
