#include <DiaCellLayers/SelectionFingerprint.h>

namespace Dia::CellLayers
{
	namespace SelectionFingerprintFn
	{
		uint64_t Mix(uint64_t cellIndex)
		{
			uint64_t z = cellIndex + 0x9E3779B97F4A7C15ull;
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
			return z ^ (z >> 31);
		}

		uint64_t HashOf(const uint32_t* indices, uint32_t count)
		{
			uint64_t h = 0;
			for (uint32_t i = 0; i < count; ++i) { h ^= Mix(indices[i]); }
			return h;
		}

		SelectionFingerprint Compute(const ICellSelectionView& selection)
		{
			uint64_t hash = 0;
			selection.ForEach([](uint32_t cellIndex, void* user) { *static_cast<uint64_t*>(user) ^= Mix(cellIndex); }, &hash);
			SelectionFingerprint fp;
			fp.mCount = selection.Count();
			fp.mVersion = selection.Version();
			fp.mHash = hash;
			return fp;
		}

		void ToHex(uint64_t hash, char (&out)[kHexLength + 1])
		{
			static const char kDigits[] = "0123456789abcdef";
			for (uint32_t i = 0; i < kHexLength; ++i)
			{
				out[kHexLength - 1 - i] = kDigits[(hash >> (4 * i)) & 0xFu];
			}
			out[kHexLength] = '\0';
		}
	}

	const SelectionFingerprint& SelectionFingerprintCache::Get(const ICellSelectionView& selection)
	{
		if (!mValid || mSource != &selection || mValue.mVersion != selection.Version())
		{
			mValue = SelectionFingerprintFn::Compute(selection);
			mSource = &selection;
			mValid = true;
		}
		return mValue;
	}
}
