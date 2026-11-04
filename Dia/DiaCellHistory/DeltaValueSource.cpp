#include <DiaCellHistory/DeltaValueSource.h>

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <chrono>
#include <cmath>

namespace Dia::CellHistory
{
	DeltaValueSource::DeltaValueSource(const CellHistoryRecorder& recorder, const HistoryViewState& view)
		: mRecorder(recorder), mView(view), mReader(recorder)
	{
	}

	DeltaValueSource::~DeltaValueSource()
	{
		delete[] mBufA;
		delete[] mBufB;
	}

	double DeltaValueSource::Now() const
	{
		if (mClockFn != nullptr) { return mClockFn(mClockCtx); }
		return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void DeltaValueSource::SetClock(double (*fn)(void*), void* ctx)
	{
		mClockFn = fn;
		mClockCtx = ctx;
	}

	float DeltaValueSource::CurrentScale(const HistoryViewState& view) const
	{
		if (!view.InDelta()) { return DeltaScale::kDefault; }
		const DeltaSpec& spec = view.Delta();
		if (!spec.autoScale) { return spec.fixedScale; }
		if (view.Epoch() != mScaleEpoch) { return DeltaScale::kDefault; }	// reset pending: nothing observed since the change
		return mScale.Current();
	}

	void DeltaValueSource::ObserveScale(const float* out, uint32_t n)
	{
		float maxAbs = 0.0f;
		bool any = false;
		for (uint32_t i = 0; i < n; ++i)
		{
			const float v = out[i];
			if (std::isnan(v)) { continue; }
			const float a = std::fabs(v);
			if (!any || a > maxAbs) { maxAbs = a; any = true; }
		}
		if (any) { mScale.Observe(maxAbs, Now()); }
	}

	void DeltaValueSource::WarnNoData(const Dia::CellLayers::CellLayerDesc& layer, const Side& side, const char* which)
	{
		if (mView.Epoch() != mWarnEpoch)
		{
			mWarnEpoch = mView.Epoch();
			mWarnedCount = 0;
		}
		for (uint32_t i = 0; i < mWarnedCount; ++i)
		{
			if (mWarned[i] == layer.id) { return; }
		}
		if (mWarnedCount < kMaxWarned) { mWarned[mWarnedCount++] = layer.id; }
		char text[80];
		FormatSide(side, text, sizeof(text));
		DIA_LOG_WARNING("HIS", "cell history delta: layer has no data on side %s (%s); no delta for it", which, text);
	}

	bool DeltaValueSource::GetValues(const Dia::CellLayers::CellLayerDesc& layer, float* out, uint32_t n)
	{
		if (!mView.InDelta() || out == nullptr || n == 0 || n != mRecorder.CellCount()) { return false; }
		if (layer.kind != Dia::CellLayers::CellValueKind::Float) { return false; }

		DIA_PROFILE_SCOPE("cellhistory.delta_read", ::Dia::Observation::Profile::Category::kDiaGraphics);

		if (mBufCount != n)
		{
			delete[] mBufA;
			delete[] mBufB;
			mBufA = new float[n];
			mBufB = new float[n];
			mBufCount = n;
		}
		const DeltaSpec& spec = mView.Delta();
		const SideReadStatus sa = mReader.Read(spec.a, layer, mBufA, n);
		if (sa != SideReadStatus::Ok) { WarnNoData(layer, spec.a, "A"); return false; }
		const SideReadStatus sb = mReader.Read(spec.b, layer, mBufB, n);
		if (sb != SideReadStatus::Ok) { WarnNoData(layer, spec.b, "B"); return false; }
		for (uint32_t i = 0; i < n; ++i)
		{
			out[i] = mBufB[i] - mBufA[i];	// NaN on either side propagates
		}
		if (layer.id == spec.layer)
		{
			if (mView.Epoch() != mScaleEpoch)
			{
				mScaleEpoch = mView.Epoch();
				mScale.Reset();
			}
			if (spec.autoScale) { ObserveScale(out, n); }
		}
		return true;
	}
}
