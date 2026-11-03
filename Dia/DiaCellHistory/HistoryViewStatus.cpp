#include <DiaCellHistory/HistoryViewStatus.h>

#include <DiaCellHistory/CellHistoryRecorder.h>

#include <stdio.h>

namespace Dia::CellHistory
{
	HistoryViewStatus DescribeHistoryView(const CellHistoryRecorder& recorder, const HistoryViewState& view)
	{
		HistoryViewStatus s;
		s.mMode = view.Mode();
		s.mFirstRetained = recorder.FirstRetained();
		const uint32_t count = recorder.SampleCount();
		if (view.Mode() == ViewMode::Sample)
		{
			s.mSample = view.Sample();
			s.mHasSample = true;
			s.mEvicted = view.IsEvicted();
		}
		else if (count > 0)
		{
			s.mSample = count - 1;
			s.mHasSample = true;
		}
		if (s.mHasSample && recorder.HasHeader(s.mSample))
		{
			snprintf(s.mLabel, sizeof(s.mLabel), "%s", recorder.Header(s.mSample).mLabel.AsCStr());
		}
		return s;
	}

	LayerViewState DescribeLayerView(const CellHistoryRecorder& recorder, const HistoryViewState& view, Dia::Core::StringCRC layer)
	{
		if (view.IsLive()) { return LayerViewState::Live; }
		if (!recorder.IsTracked(layer)) { return LayerViewState::Untracked; }
		if (view.IsEvicted()) { return LayerViewState::Evicted; }
		return LayerViewState::HasData;
	}

	const char* LayerViewStateName(LayerViewState state)
	{
		switch (state)
		{
		case LayerViewState::Live:		return "live";
		case LayerViewState::HasData:	return "data";
		case LayerViewState::Untracked:	return "untracked";
		case LayerViewState::Evicted:	return "evicted";
		}
		return "live";
	}
}
