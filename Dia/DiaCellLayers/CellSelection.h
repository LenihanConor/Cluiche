#pragma once

// CellSelection: a set of cells on one grid (bitset + cached count + version). Cell identity is the grid index
// (row-major: r * width + q). The version increments on every change and not otherwise. Binding to a different grid
// (pointer, width, height or topology id differs) clears the selection. Out-of-range cells are ignored.
// BeginEdit/EndEdit batch many edits into at most one version bump (net change only).
// The grid pointer is borrowed. Generic: no game types (CL-005), no singletons (CL-006).

#include <DiaGraphics/CellField/CellGridDesc.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	class CellSelection
	{
	public:
		CellSelection() = default;
		~CellSelection();
		CellSelection(const CellSelection&) = delete;
		CellSelection& operator=(const CellSelection&) = delete;

		// Binds to a grid (null = none). Same grid (pointer, width, height, topology) is a no-op; otherwise the
		// selection is resized and cleared (version bumps only if it was non-empty).
		void		Reset(const Dia::Graphics::CellGridDesc* grid);

		uint32_t	Width() const { return mWidth; }
		uint32_t	Height() const { return mHeight; }
		uint32_t	CellCount() const { return mWidth * mHeight; }
		float		CellSize() const { return mCellSize; }	// grid cellSize (centre-to-corner) at last Reset; 0 if no grid. Not part of grid identity.

		bool		Contains(uint32_t cellIndex) const;
		bool		Set(uint32_t cellIndex, bool selected);		// true if the state changed
		bool		Clear();									// true if it was non-empty

		uint32_t	Count() const { return mCount; }
		uint32_t	Version() const { return mVersion; }

		// Visits set cells in ascending index order.
		void		ForEach(void (*fn)(uint32_t cellIndex, void* user), void* user) const;

		// Bulk edit: Set/Clear between Begin and End do not bump; EndEdit bumps once iff the set differs from at
		// BeginEdit. Not nestable (a nested BeginEdit is ignored).
		void		BeginEdit();
		bool		EndEdit();
		bool		IsEditing() const { return mEditing; }

	private:
		uint32_t	WordCount() const { return (CellCount() + 63u) / 64u; }
		void		Bump() { if (!mEditing) { ++mVersion; } }

		const Dia::Graphics::CellGridDesc*	mGrid = nullptr;
		Dia::Core::StringCRC				mTopology;
		uint32_t							mWidth = 0;
		uint32_t							mHeight = 0;
		float								mCellSize = 0.0f;
		uint64_t*							mWords = nullptr;
		uint64_t*							mSnapshot = nullptr;
		uint32_t							mCount = 0;
		uint32_t							mVersion = 0;
		bool								mEditing = false;
	};
}
