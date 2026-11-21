#include <DiaCellLayers/CellSelection.h>

#include <string.h>

namespace Dia::CellLayers
{
	CellSelection::~CellSelection()
	{
		delete[] mWords;
		delete[] mSnapshot;
	}

	void CellSelection::Reset(const Dia::Graphics::CellGridDesc* grid)
	{
		const uint32_t width = grid ? grid->width : 0u;
		const uint32_t height = grid ? grid->height : 0u;
		const Dia::Core::StringCRC topology = grid ? grid->topology : Dia::Core::StringCRC();

		mCellSize = grid ? grid->cellSize : 0.0f;	// refreshed even when the grid identity is unchanged

		if (grid == mGrid && width == mWidth && height == mHeight && topology == mTopology)
		{
			return;
		}

		const bool wasNonEmpty = mCount > 0;
		const uint32_t oldWords = WordCount();

		mGrid = grid;
		mTopology = topology;
		mWidth = width;
		mHeight = height;

		// A rebind cancels any edit in progress; the bump below covers the change.
		mEditing = false;
		delete[] mSnapshot;
		mSnapshot = nullptr;

		const uint32_t newWords = WordCount();
		if (newWords != oldWords)
		{
			delete[] mWords;
			mWords = newWords > 0 ? new uint64_t[newWords] : nullptr;
		}
		if (mWords)
		{
			memset(mWords, 0, newWords * sizeof(uint64_t));
		}
		mCount = 0;

		if (wasNonEmpty)
		{
			++mVersion;
		}
	}

	bool CellSelection::Contains(uint32_t cellIndex) const
	{
		if (cellIndex >= CellCount())
		{
			return false;
		}
		return (mWords[cellIndex >> 6] >> (cellIndex & 63u)) & 1u;
	}

	bool CellSelection::Set(uint32_t cellIndex, bool selected)
	{
		if (cellIndex >= CellCount() || Contains(cellIndex) == selected)
		{
			return false;
		}
		const uint64_t bit = uint64_t(1) << (cellIndex & 63u);
		if (selected)
		{
			mWords[cellIndex >> 6] |= bit;
			++mCount;
		}
		else
		{
			mWords[cellIndex >> 6] &= ~bit;
			--mCount;
		}
		Bump();
		return true;
	}

	bool CellSelection::Clear()
	{
		if (mCount == 0)
		{
			return false;
		}
		memset(mWords, 0, WordCount() * sizeof(uint64_t));
		mCount = 0;
		Bump();
		return true;
	}

	void CellSelection::ForEach(void (*fn)(uint32_t cellIndex, void* user), void* user) const
	{
		if (!fn)
		{
			return;
		}
		const uint32_t words = WordCount();
		for (uint32_t w = 0; w < words; ++w)
		{
			uint64_t bits = mWords[w];
			while (bits)
			{
				uint32_t b = 0;
				while (((bits >> b) & 1u) == 0)
				{
					++b;
				}
				fn(w * 64u + b, user);
				bits &= bits - 1;
			}
		}
	}

	void CellSelection::BeginEdit()
	{
		if (mEditing)
		{
			return;
		}
		mEditing = true;
		const uint32_t words = WordCount();
		delete[] mSnapshot;
		mSnapshot = nullptr;
		if (words > 0)
		{
			mSnapshot = new uint64_t[words];
			memcpy(mSnapshot, mWords, words * sizeof(uint64_t));
		}
	}

	bool CellSelection::EndEdit()
	{
		if (!mEditing)
		{
			return false;
		}
		mEditing = false;
		const uint32_t words = WordCount();
		const bool changed = words > 0 && memcmp(mSnapshot, mWords, words * sizeof(uint64_t)) != 0;
		delete[] mSnapshot;
		mSnapshot = nullptr;
		if (changed)
		{
			++mVersion;
		}
		return changed;
	}
}
