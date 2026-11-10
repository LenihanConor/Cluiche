#pragma once

// SelectionFingerprint: count + version + 64-bit order-independent hash of a cell selection's indices (editor mirror,
// Feature 5). hash = XOR of Mix(i) over every selected cell index i (row-major r * width + q); Mix is the SplitMix64
// finaliser (all arithmetic modulo 2^64). Empty selection hashes to 0. The hex form is 16 lower-case digits.
// Cost is O(selected cells); callers cache per selection version (see SelectionFingerprintCache).
// Generic: no game types (CL-005), no singletons (CL-006). No STL in the API (PD-004).

#include <DiaCellLayers/ICellSelectionView.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	struct SelectionFingerprint
	{
		uint32_t	mCount = 0;
		uint32_t	mVersion = 0;
		uint64_t	mHash = 0;

		bool operator==(const SelectionFingerprint& o) const { return mCount == o.mCount && mVersion == o.mVersion && mHash == o.mHash; }
		bool operator!=(const SelectionFingerprint& o) const { return !(*this == o); }
	};

	namespace SelectionFingerprintFn
	{
		constexpr uint32_t kHexLength = 16;

		uint64_t				Mix(uint64_t cellIndex);						// SplitMix64 finaliser of (i + golden gamma)
		uint64_t				HashOf(const uint32_t* indices, uint32_t count);	// order-independent
		SelectionFingerprint	Compute(const ICellSelectionView& selection);
		void					ToHex(uint64_t hash, char (&out)[kHexLength + 1]);	// 16 lower-case digits + NUL
	}

	// Recomputes only when the selection's version (or identity of the view) changes.
	class SelectionFingerprintCache
	{
	public:
		const SelectionFingerprint&	Get(const ICellSelectionView& selection);
		void						Invalidate() { mValid = false; }

	private:
		SelectionFingerprint	mValue;
		const ICellSelectionView*	mSource = nullptr;
		bool					mValid = false;
	};
}
