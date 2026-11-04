#pragma once

// DeltaValueSource: value computation for delta mode (Feature 4). Owned and used by HistoryValueSource; it is NOT installed
// on the registry itself. For the view's DeltaSpec it reads side A and side B of a float layer through SideReader and
// returns B - A per cell. A cell is NaN when either side's cell is NaN. A category layer, a layer with no data on a side
// (NoData) and an Unavailable side have no delta: GetValues returns false (never a fallback to live values; `now` is a
// legitimate side, not a fallback). Scale is not applied to the values; CurrentScale() reports the legend scale: the view's fixed
// scale when fixed, else the DeltaScale auto value (largest |delta| over non-NaN cells of the delta layer, nice-rounded; see
// DeltaScale.h), observed on each successful GetValues of the delta layer and reset whenever the view epoch changes (a new
// delta, or fixed/auto switched), NOT when the viewed data changes. The clock is injectable (default: steady wall clock). Quantisation: a delta between two recorded sides has error at
// most 2 * range / 65,535; a `now` side is exact.
// One Warning (channel HIS) is logged per layer per view epoch when a side has no data. Two scratch buffers of CellCount
// floats are allocated on first use and reused. Read-only over the recorder and view state (borrowed; must outlive this).
// Same-PU contract as the recorder. No singletons.

#include <DiaCellHistory/DeltaScale.h>
#include <DiaCellHistory/SideReader.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>
#include <memory>

namespace Dia::CellLayers { struct CellLayerDesc; struct ViewPresentation; }
namespace Dia::ValueSchema { class ValueSchemaService; class RampResolver; }

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

		// Legend scale for the view's delta: fixedScale when fixed, else the auto scale (1.0 until observed). 1.0 when not in delta.
		float CurrentScale(const HistoryViewState& view) const;

		// True when the view's delta uses the auto scale (false when fixed or not in delta). The panel's "auto"/"fixed" label.
		bool IsAutoScale(const HistoryViewState& view) const;

		// Optional schema: lets GetPresentation resolve the layer's ui.delta_ramp. Borrowed; must outlive this. nullptr clears.
		void SetSchema(const Dia::ValueSchema::ValueSchemaService* schema);

		// Delta presentation for the viewer: the layer's delta ramp (ui.delta_ramp, default diverging.blue_red), range
		// [-s, +s] with s = CurrentScale, unit prefix "\xCE\x94" (UTF-8 Greek capital delta). False when not in delta, for a
		// non-Float layer, with no schema, or when the layer has no ValueInfo (the viewer then keeps schema colouring).
		// Any Float layer gets it in delta mode: the viewer only asks for its active base layer.
		bool GetPresentation(const Dia::CellLayers::CellLayerDesc& layer, Dia::CellLayers::ViewPresentation& out);

		// Seconds source for the 2 s shrink rule. nullptr restores the default monotonic wall clock.
		void SetClock(double (*fn)(void*), void* ctx);

	private:
		static const uint32_t kMaxWarned = 128;

		double Now() const;
		void ObserveScale(const float* out, uint32_t n);

		void WarnNoData(const Dia::CellLayers::CellLayerDesc& layer, const Side& side, const char* which);

		const CellHistoryRecorder&	mRecorder;
		const HistoryViewState&		mView;
		SideReader					mReader;
		float*						mBufA = nullptr;
		float*						mBufB = nullptr;
		uint32_t					mBufCount = 0;
		const Dia::ValueSchema::ValueSchemaService*			mSchema = nullptr;
		std::unique_ptr<Dia::ValueSchema::RampResolver>		mResolver;
		DeltaScale					mScale;
		uint64_t					mScaleEpoch = 0;
		double						(*mClockFn)(void*) = nullptr;
		void*						mClockCtx = nullptr;
		uint64_t					mWarnEpoch = 0;
		uint32_t					mWarnedCount = 0;
		Dia::Core::StringCRC		mWarned[kMaxWarned];
	};
}
