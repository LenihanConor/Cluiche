#include <DiaCellHistory/ViewHistorySourceAdapter.h>
#include <DiaCellHistory/TimelineCardState.h>

namespace Dia::CellHistory
{
	Json::Value ViewHistorySourceAdapter::HistoryField() const
	{
		return TimelineCardState::HistoryField(&mRecorder, &mView);
	}

	uint64_t ViewHistorySourceAdapter::HistoryStamp() const
	{
		return mView.Epoch() * 2 + (mView.Mode() != ViewMode::Live ? 1 : 0);
	}
}
