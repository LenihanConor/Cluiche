#pragma once

// DeltaValueSource: value computation for delta mode (Feature 4). Owned and used by HistoryValueSource; it is NOT installed
// on the registry itself. For the view's DeltaSpec it reads side A and side B of a float layer through SideReader and
// returns B - A per cell. A cell is NaN when either side's cell is NaN. A category layer, a layer with no data on a side
// (NoData) and an Unavailable side have no delta: GetValues returns false (never a fallback to live values; `now` is a
// legitimate side, not a fallback). Scale is not applied. Quantisation: a delta between two recorded sides has error at
// most 2 * range / 65,535; a `now` side is exact.
// One Warning (channel HIS) is logged per layer per view epoch when a side has no data. Two scratch buffers of CellCount
// floats are allocated on first use and reused. Read-only over the recorder and view state (borrowed; must outlive this).
// Same-PU contract as the recorder. No singletons.

#include <DiaCellHistory/SideReader.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Dia::CellLayers { struct CellLayerDesc; }

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class HistoryViewState;

	class DeltaValueSource
	{
	public:
		DeltaValueSource(const CellHistoryRecorder& recorder, const HistoryViewState& view);
		~DeltaValueSource();
		DeltaValueSource(const DeltaValueSource&) = delete;
		DeltaValueSource& operator=(const DeltaValueSource&) = delete;

		// Fills out[0..n) with B - A. False (out untouched) when not in delta, null out, n != CellCount, n == 0, a category
		// layer, or a side without data.
		bool GetValues(const Dia::CellLayers::CellLayerDesc& layer, float* out, uint32_t n);

	private:
		static const uint32_t kMaxWarned = 128;

		void WarnNoData(const Dia::CellLayers::CellLayerDesc& layer, const Side& side, const char* which);

		const CellHistoryRecorder&	mRecorder;
		const HistoryViewState&		mView;
		SideReader					mReader;
		float*						mBufA = nullptr;
		float*						mBufB = nullptr;
		uint32_t					mBufCount = 0;
		uint64_t					mWarnEpoch = 0;
		uint32_t					mWarnedCount = 0;
		Dia::Core::StringCRC		mWarned[kMaxWarned];
	};
}
