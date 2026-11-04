#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <cmath>
#include <cstdio>

namespace Dia::CellHistory
{
	void FormatSide(const Side& side, char* buf, uint32_t n)
	{
		if (buf == nullptr || n == 0) { return; }
		buf[0] = '\0';
		switch (side.kind)
		{
		case SideKind::Now:			std::snprintf(buf, n, "now"); break;
		case SideKind::Sample:		std::snprintf(buf, n, "sample:%u", side.sample); break;
		case SideKind::Snapshot:	std::snprintf(buf, n, "snapshot:%s", side.snapshot.AsCStr()); break;
		}
	}

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
		DIA_PROFILE_SCOPE("cellhistory.view_sample", ::Dia::Observation::Profile::Category::kDiaGraphics);

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
		DIA_PROFILE_SCOPE("cellhistory.go_live", ::Dia::Observation::Profile::Category::kDiaGraphics);

		if (mMode == ViewMode::Live)
		{
			return;
		}
		mMode = ViewMode::Live;
		mEvictedSeen = false;
		++mEpoch;
		DIA_LOG_INFO("HIS", "cell history view: back to live");
	}

	void HistoryViewState::RestoreHeld()
	{
		if (mHeldMode == ViewMode::Sample && mHeldSample < mRecorder.SampleCount())
		{
			mMode = ViewMode::Sample;
			mSample = mHeldSample;
			mEvictedSeen = IsEvicted();
		}
		else
		{
			mMode = ViewMode::Live;
			mEvictedSeen = false;
		}
		++mEpoch;
	}

	bool HistoryViewState::EnterDelta(Dia::Core::StringCRC layer, const Side& a, const Side& b, ViewResult& out, bool autoScale, float fixedScale)
	{
		DIA_PROFILE_SCOPE("cellhistory.enter_delta", ::Dia::Observation::Profile::Category::kDiaGraphics);

		out.mOk = false;
		out.mError[0] = '\0';
		const uint32_t count = mRecorder.SampleCount();
		if (SidesEqual(a, b))
		{
			std::snprintf(out.mError, ViewResult::kErrorChars, "delta sides are identical");
		}
		else if (a.kind == SideKind::Sample && a.sample >= count)
		{
			std::snprintf(out.mError, ViewResult::kErrorChars, "sample %u is out of range (%u recorded)", a.sample, count);
		}
		else if (b.kind == SideKind::Sample && b.sample >= count)
		{
			std::snprintf(out.mError, ViewResult::kErrorChars, "sample %u is out of range (%u recorded)", b.sample, count);
		}
		else if (!autoScale && !(std::isfinite(fixedScale) && fixedScale > 0.0f))
		{
			std::snprintf(out.mError, ViewResult::kErrorChars, "delta scale must be finite and greater than 0");
		}
		if (out.mError[0] != '\0')
		{
			DIA_LOG_ERROR("HIS", "cell history delta: %s", out.mError);
			return false;
		}
		out.mOk = true;
		if (mMode == ViewMode::Delta && mDelta.layer == layer && SidesEqual(mDelta.a, a) && SidesEqual(mDelta.b, b)
			&& mDelta.autoScale == autoScale && (autoScale || mDelta.fixedScale == fixedScale))
		{
			return true;	// no change
		}
		if (mMode != ViewMode::Delta)
		{
			mHeldMode = mMode;
			mHeldSample = (mMode == ViewMode::Sample) ? mSample : 0;
		}
		mMode = ViewMode::Delta;
		mDelta.layer = layer;
		mDelta.a = a;
		mDelta.b = b;
		mDelta.autoScale = autoScale;
		mDelta.fixedScale = autoScale ? 0.0f : fixedScale;
		mEvictedSeen = false;
		++mEpoch;
		char ta[80], tb[80];
		FormatSide(a, ta, sizeof(ta));
		FormatSide(b, tb, sizeof(tb));
		DIA_LOG_INFO("HIS", "cell history view: delta mode %s vs %s (%s scale)", ta, tb, autoScale ? "auto" : "fixed");
		return true;
	}

	bool HistoryViewState::ExitDelta()
	{
		if (mMode != ViewMode::Delta) { return false; }
		RestoreHeld();
		DIA_LOG_INFO("HIS", "cell history view: left delta mode, back to %s", mMode == ViewMode::Live ? "live" : "scrub");
		return true;
	}

	bool HistoryViewState::SetDeltaScaleFixed(float scale, ViewResult& out)
	{
		out.mOk = false;
		out.mError[0] = '\0';
		if (mMode != ViewMode::Delta)
		{
			std::snprintf(out.mError, ViewResult::kErrorChars, "not in delta mode");
		}
		else if (!(std::isfinite(scale) && scale > 0.0f))
		{
			std::snprintf(out.mError, ViewResult::kErrorChars, "delta scale must be finite and greater than 0");
		}
		if (out.mError[0] != '\0')
		{
			DIA_LOG_ERROR("HIS", "cell history delta: %s", out.mError);
			return false;
		}
		out.mOk = true;
		if (!mDelta.autoScale && mDelta.fixedScale == scale) { return true; }
		mDelta.autoScale = false;
		mDelta.fixedScale = scale;
		++mEpoch;
		DIA_LOG_INFO("HIS", "cell history view: delta scale fixed at +/-%g", static_cast<double>(scale));
		return true;
	}

	bool HistoryViewState::SetDeltaScaleAuto(ViewResult& out)
	{
		out.mOk = false;
		out.mError[0] = '\0';
		if (mMode != ViewMode::Delta)
		{
			std::snprintf(out.mError, ViewResult::kErrorChars, "not in delta mode");
			DIA_LOG_ERROR("HIS", "cell history delta: %s", out.mError);
			return false;
		}
		out.mOk = true;
		if (mDelta.autoScale) { return true; }
		mDelta.autoScale = true;
		mDelta.fixedScale = 0.0f;
		++mEpoch;
		DIA_LOG_INFO("HIS", "cell history view: delta scale set to auto");
		return true;
	}

	bool HistoryViewState::Update()
	{
		if (mMode == ViewMode::Delta)
		{
			const uint32_t count = mRecorder.SampleCount();
			const bool sideGone = (mDelta.a.kind == SideKind::Sample && mDelta.a.sample >= count)
				|| (mDelta.b.kind == SideKind::Sample && mDelta.b.sample >= count);
			if (!sideGone) { return false; }	// eviction of a side is not a state change
			DIA_LOG_WARNING("HIS", "cell history view: a delta side was truncated away (%u recorded); leaving delta mode", count);
			RestoreHeld();
			return true;
		}
		if (mMode == ViewMode::Sample && mSample >= mRecorder.SampleCount())
		{
			// The recorder was truncated below the viewed sample (e.g. a restore): it no longer exists, so follow live.
			DIA_LOG_WARNING("HIS", "cell history view: sample %u was truncated away (%u recorded); back to live", mSample, mRecorder.SampleCount());
			mMode = ViewMode::Live;
			mEvictedSeen = false;
			++mEpoch;
			return true;
		}
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
