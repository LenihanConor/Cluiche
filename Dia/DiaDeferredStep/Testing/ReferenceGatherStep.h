#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/IParallelExecutor.h>

namespace Dia::DeferredStep::Testing
{
	// Reference step for determinism tests. Pass 1 is a gather: each output cell is a function of its input
	// neighbourhood and is written to its own slot. Pass 2 sums each row of outputs (per-row slots); the grand
	// total is then folded serially in row order with a non-commutative mix, so any ordering bug changes the bytes.
	// All integer maths; a pure function of the seed and cell count.
	class ReferenceGatherStep final : public IDeferredStep
	{
	public:
		static constexpr uint32_t kRowWidth = 16;

		ReferenceGatherStep(uint32_t cellCount, uint32_t seed) : mCellCount(cellCount), mSeed(seed) {}

		void Capture() override
		{
			mInput.assign(mCellCount, 0);
			uint32_t s = mSeed * 2654435761u + 12345u;
			for (uint32_t i = 0; i < mCellCount; ++i)
			{
				s ^= s << 13; s ^= s >> 17; s ^= s << 5;
				mInput[i] = s;
			}
			mGathered.assign(mCellCount, 0);
			mRowSums.assign(RowCount(), 0);
			mTotal = 0;
		}

		void Run(IParallelExecutor& executor) override
		{
			const uint32_t n = mCellCount;
			executor.ParallelFor(n, 1, [this, n](uint32_t b, uint32_t e)
			{
				for (uint32_t i = b; i < e; ++i)
				{
					const uint32_t l = mInput[i == 0 ? 0 : i - 1];
					const uint32_t c = mInput[i];
					const uint32_t r = mInput[i + 1 >= n ? n - 1 : i + 1];
					mGathered[i] = (l * 31u) ^ (c * 2654435761u) ^ (r >> 3) ^ i;
				}
			});

			const uint32_t rows = RowCount();
			executor.ParallelFor(rows, 1, [this, n](uint32_t b, uint32_t e)
			{
				for (uint32_t row = b; row < e; ++row)
				{
					uint64_t sum = 0;
					const uint32_t first = row * kRowWidth;
					const uint32_t last = first + kRowWidth < n ? first + kRowWidth : n;
					for (uint32_t i = first; i < last; ++i) { sum += mGathered[i]; }
					mRowSums[row] = sum;
				}
			});

			uint64_t total = 14695981039346656037ull;
			for (uint32_t row = 0; row < rows; ++row) { total = (total ^ mRowSums[row]) * 1099511628211ull + row; }
			mTotal = total;
		}

		void Apply() override {}

		uint32_t GetPendingVersion() const override { return 1; }
		void     SerializePending(Dia::SaveGame::SaveContext&) const override {}
		void     DeserializePending(Dia::SaveGame::LoadContext&) override {}

		// Result as raw bytes for memcmp: gathered cells, row sums, total.
		std::vector<uint8_t> GetResultBytes() const
		{
			std::vector<uint8_t> out;
			Append(out, mGathered.data(), mGathered.size() * sizeof(uint32_t));
			Append(out, mRowSums.data(), mRowSums.size() * sizeof(uint64_t));
			Append(out, &mTotal, sizeof(mTotal));
			return out;
		}

	private:
		uint32_t RowCount() const { return (mCellCount + kRowWidth - 1) / kRowWidth; }
		static void Append(std::vector<uint8_t>& out, const void* p, size_t bytes)
		{
			const uint8_t* b = static_cast<const uint8_t*>(p);
			out.insert(out.end(), b, b + bytes);
		}

		uint32_t              mCellCount;
		uint32_t              mSeed;
		std::vector<uint32_t> mInput;
		std::vector<uint32_t> mGathered;
		std::vector<uint64_t> mRowSums;
		uint64_t              mTotal = 0;
	};
}
