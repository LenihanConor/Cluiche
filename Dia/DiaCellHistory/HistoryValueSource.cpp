#include <DiaCellHistory/HistoryValueSource.h>

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/CellLayerRegistry.h>

namespace Dia::CellHistory
{
	HistoryValueSource::HistoryValueSource(Dia::CellLayers::CellLayerRegistry& registry, const CellHistoryRecorder& recorder, HistoryViewState& view)
		: mRegistry(registry), mRecorder(recorder), mView(view), mDelta(recorder, view)
	{
	}

	HistoryValueSource::~HistoryValueSource() { Uninstall(); }

	bool HistoryValueSource::Install()
	{
		if (mInstalled) { return true; }
		mInstalled = mRegistry.SetViewValueSource(this);
		return mInstalled;
	}

	void HistoryValueSource::Uninstall()
	{
		if (!mInstalled) { return; }
		if (mRegistry.ViewValueSource() == this) { mRegistry.ClearViewValueSource(); }
		mInstalled = false;
	}

	bool HistoryValueSource::Tick() { return mView.Update(); }

	bool HistoryValueSource::GetValues(const Dia::CellLayers::CellLayerDesc& layer, float* out, uint32_t n)
	{
		if (mView.InDelta()) { return mDelta.GetValues(layer, out, n); }
		if (mView.IsLive() || out == nullptr || n != mRecorder.CellCount() || n == 0) { return false; }
		if (mView.Sample() < mRecorder.FirstRetained()) { return false; }
		return mRecorder.CellValues(mView.Sample(), layer.id, out, n);
	}

	bool HistoryValueSource::GetPresentation(const Dia::CellLayers::CellLayerDesc& layer, Dia::CellLayers::ViewPresentation& out)
	{
		return mView.InDelta() && mDelta.GetPresentation(layer, out);
	}

	bool HistoryValueSource::IsLive() const { return mView.IsLive(); }
	uint64_t HistoryValueSource::ViewEpoch() const { return mView.Epoch(); }
}
