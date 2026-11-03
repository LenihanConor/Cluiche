#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstdio>

namespace Dia::CellHistory
{
	HistoryViewState::HistoryViewState(const CellHistoryRecorder& recorder)
		: mRecorder(recorder)
	{
	}

	bool HistoryViewState::IsEvicted() const
	{
		return mMode == ViewMode::Sample && mSample < mRecorder.FirstRetained();
	}

	bool HistoryViewState::ViewSample(uint32_t sample, ViewResult& out)
	{
		out.mOk = false;
		out.mError[0] = '\0';
		const uint32_t count = mRecorder.SampleCount();
		if (sample >= count)
		{
			std::snprintf(out.mError, ViewResult::kErrorChars, "sample %u is out of range (%u recorded)", sample, count);
			DIA_LOG_ERROR("HIS", "cell history view: %s", out.mError);
			return false;
		}
		out.mOk = true;
		if (mMode == ViewMode::Sample && mSample == sample)
		{
			return true;	// no change
		}
		mMode = ViewMode::Sample;
		mSample = sample;
		mEvictedSeen = IsEvicted();
		++mEpoch;
		const char* label = mRecorder.HasHeader(sample) ? mRecorder.Header(sample).mLabel.AsCStr() : "";
		DIA_LOG_INFO("HIS", "cell history view: scrub mode on sample %u ('%s')%s", sample, label, mEvictedSeen ? " (per-cell values evicted)" : "");
		return true;
	}

	void HistoryViewState::GoLive()
	{
		if (mMode == ViewMode::Live)
		{
			return;
		}
		mMode = ViewMode::Live;
		mEvictedSeen = false;
		++mEpoch;
		DIA_LOG_INFO("HIS", "cell history view: back to live");
	}

	bool HistoryViewState::Update()
	{
		const bool evicted = IsEvicted();
		if (evicted == mEvictedSeen)
		{
			return false;
		}
		mEvictedSeen = evicted;
		++mEpoch;
		if (evicted)
		{
			DIA_LOG_WARNING("HIS", "cell history view: sample %u evicted from the per-cell window (first retained %u); view stays on it", mSample, mRecorder.FirstRetained());
		}
		return evicted;
	}
}
