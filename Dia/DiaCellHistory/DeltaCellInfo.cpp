#include <DiaCellHistory/DeltaCellInfo.h>

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <cmath>
#include <cstdio>
#include <limits>

namespace Dia::CellHistory
{
	namespace
	{
		const char* const kProviderName = "History delta";
	}

	DeltaCellReader::DeltaCellReader(const CellHistoryRecorder& recorder) : mRecorder(recorder), mReader(recorder) {}

	DeltaCellReader::~DeltaCellReader()
	{
		delete[] mBufA;
		delete[] mBufB;
	}

	bool DeltaCellReader::Get(const Dia::CellLayers::CellLayerDesc& layer, const DeltaSpec& delta, uint64_t viewEpoch, uint32_t cell, DeltaCellInfo& out)
	{
		const uint32_t n = mRecorder.CellCount();
		if (cell >= n) { return false; }
		const uint64_t version = layer.version ? layer.version() : 0;
		const uint32_t firstRetained = mRecorder.FirstRetained();
		if (!mValid || mKeyEpoch != viewEpoch || mKeyVersion != version || mKeyFirstRetained != firstRetained || !(mKeyLayer == layer.id))
		{
			DIA_PROFILE_SCOPE("cellhistory.delta_cell_read", ::Dia::Observation::Profile::Category::kDiaGraphics);
			if (mBufCount != n)
			{
				delete[] mBufA; delete[] mBufB;
				mBufA = new float[n];
				mBufB = new float[n];
				mBufCount = n;
			}
			mOkA = mReader.Read(delta.a, layer, mBufA, n) == SideReadStatus::Ok;
			mOkB = mReader.Read(delta.b, layer, mBufB, n) == SideReadStatus::Ok;
			mReads += 2;
			mKeyEpoch = viewEpoch;
			mKeyVersion = version;
			mKeyFirstRetained = firstRetained;
			mKeyLayer = layer.id;
			mValid = true;
		}
		out.aHasData = mOkA;
		out.bHasData = mOkB;
		out.a = mOkA ? mBufA[cell] : 0.0f;
		out.b = mOkB ? mBufB[cell] : 0.0f;
		const bool isFloat = layer.kind == Dia::CellLayers::CellValueKind::Float;
		out.delta = (mOkA && mOkB && isFloat) ? out.b - out.a : std::numeric_limits<float>::quiet_NaN();
		return true;
	}

	DeltaCellDetails::DeltaCellDetails(const CellHistoryRecorder& recorder, const HistoryViewState& view)
		: mRecorder(recorder), mView(view), mCells(recorder)
	{
	}

	DeltaCellDetails::~DeltaCellDetails() { Unregister(); }

	void DeltaCellDetails::Unregister()
	{
		if (mGrid != nullptr && mRegistry != nullptr) { mRegistry->Details().Unregister(mGrid, kProviderName); }
		mGrid = nullptr;
	}

	void DeltaCellDetails::Sync(Dia::CellLayers::CellLayerRegistry* registry)
	{
		const Dia::CellLayers::CellLayerDesc* layer = (registry != nullptr && mView.InDelta()) ? registry->Find(mView.Delta().layer) : nullptr;
		const Dia::Graphics::CellGridDesc* want = layer != nullptr ? layer->grid : nullptr;
		if (want == mGrid && registry == mRegistry) { return; }
		Unregister();
		mRegistry = registry;
		if (want == nullptr) { return; }
		if (registry->Details().Register(want, kProviderName, Dia::CellLayers::CellDetailProviderFn(this, &DeltaCellDetails::Provide)) == Dia::CellLayers::CellDetailRegisterResult::Ok)
		{
			mGrid = want;
		}
	}

	void DeltaCellDetails::Provide(uint32_t cellIndex, Dia::CellLayers::CellDetailBuilder& builder)
	{
		if (mRegistry == nullptr || !mView.InDelta()) { return; }
		const Dia::CellLayers::CellLayerDesc* layer = mRegistry->Find(mView.Delta().layer);
		DeltaCellInfo info;
		if (layer == nullptr || !mCells.Get(*layer, mView.Delta(), mView.Epoch(), cellIndex, info)) { return; }
		char side[80], label[96], text[48];
		auto value = [&](float v, bool has, const char* what, const Side& s)
		{
			FormatSide(s, side, sizeof(side));
			std::snprintf(label, sizeof(label), "%s (%s)", what, side);
			if (has) { std::snprintf(text, sizeof(text), "%.4g", static_cast<double>(v)); }
			else { std::snprintf(text, sizeof(text), "no data"); }
			builder.Row(label, text);
		};
		value(info.a, info.aHasData, "A", mView.Delta().a);
		value(info.b, info.bHasData, "B", mView.Delta().b);
		if (std::isnan(info.delta)) { std::snprintf(text, sizeof(text), "no data"); }
		else { std::snprintf(text, sizeof(text), "%+.4g", static_cast<double>(info.delta)); }
		builder.Row("Delta", text);
	}
}
