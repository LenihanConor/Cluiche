#pragma once

// ViewHistorySourceAdapter: feeds the "history" value of cells.view.state from the recorder and the history view.
// Install on ViewStatePublisher::SetHistorySource. Borrows both (must outlive the adapter); same-PU contract (SimPU).
//
// HistoryField() is TimelineCardState::HistoryField(recorder, view). HistoryStamp() is view.Epoch() * 2 + (Sample mode
// ? 1 : 0). The field depends only on the view's mode, viewed sample and that sample's label, and Epoch() already bumps on
// every mode or sample change (and on eviction), while a recorded sample's label never changes. The recorder's growing
// SampleCount() does not alter the field (live has null sample/label), so it is deliberately NOT in the stamp: including
// it would republish the topic on every new sample for no change.

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/IViewHistorySource.h>

namespace Dia::CellHistory
{
	class ViewHistorySourceAdapter : public Dia::CellLayers::IViewHistorySource
	{
	public:
		ViewHistorySourceAdapter(const CellHistoryRecorder& recorder, const HistoryViewState& view)
			: mRecorder(recorder), mView(view) {}

		Json::Value	HistoryField() const override;
		uint64_t	HistoryStamp() const override;

	private:
		const CellHistoryRecorder&	mRecorder;
		const HistoryViewState&		mView;
	};
}
