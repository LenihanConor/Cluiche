#include <DiaCellHistory/DeltaValueSource.h>

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

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
		return true;
	}
}
