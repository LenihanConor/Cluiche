#pragma once

// HistoryViewState: which recorded sample (if any) the map is showing. Live follows the simulation; Sample(n) is scrub
// mode, pinned to recorded sample n until GoLive(). Viewing the latest sample is scrub mode, NOT live.
// Read-only over the recorder. Same-PU contract as the recorder. No singletons.
// Epoch() changes whenever the shown view changes (mode, sample, or the viewed sample falling out of the per-cell
// window); Update() must be called once per frame (or before reading) to detect the window passing the viewed sample.
// Delta(layer, A, B, scale) (Feature 4) is a third mode: it remembers the Live/Sample view held before it and returns to it on
// ExitDelta(). Sample() in delta mode is the held sample. IsLive() and IsEvicted() are false in delta mode.

#include <DiaCellHistory/SideReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;

	enum class ViewMode : uint8_t { Live, Sample, Delta };

	struct DeltaSpec
	{
		Dia::Core::StringCRC	layer;
		Side					a;
		Side					b;
		bool					autoScale = true;
		float					fixedScale = 0.0f;	// > 0 when !autoScale
		bool					groupSideB = true;	// which side's grouping defines the group table's cells; reset to B on each entry
	};

	// "now", "sample:3" or "snapshot:name" into buf (terminated for n >= 1).
	void FormatSide(const Side& side, char* buf, uint32_t n);

	struct ViewResult
	{
		static constexpr uint32_t kErrorChars = 128;
		bool	mOk = false;
		char	mError[kErrorChars] = {};	// when !mOk
	};

	class HistoryViewState
	{
	public:
		explicit HistoryViewState(const CellHistoryRecorder& recorder);

		// Enters (or moves) scrub mode on `sample`. sample >= SampleCount() is an error and changes nothing.
		bool ViewSample(uint32_t sample, ViewResult& out);
		void GoLive();											// no-op when already live

		// Re-evaluates eviction against the recorder: when the per-cell window first passes the viewed sample the view
		// stays, the epoch bumps and ONE Warning is logged. Returns true if that transition happened on this call.
		// If the recorder was truncated below the viewed sample the view returns to live (epoch bumps, one Warning) and this
		// also returns true.
		bool Update();

		// Delta mode. EnterDelta rejects identical sides, an out-of-range sample side and a non-finite or <= 0 fixed scale (nothing
		// changes). Entering while in delta replaces the spec and keeps the originally held view; re-entering an identical spec is a no-op.
		bool EnterDelta(Dia::Core::StringCRC layer, const Side& a, const Side& b, ViewResult& out, bool autoScale = true, float fixedScale = 0.0f);
		bool ExitDelta();										// back to the held view; false (no-op) when not in delta
		bool SetDeltaScaleFixed(float scale, ViewResult& out);	// finite and > 0, only in delta
		bool SetDeltaScaleAuto(ViewResult& out);				// only in delta
		bool SetDeltaGroupSide(bool useB, ViewResult& out);		// only in delta; bumps the epoch only when it changes
		bool				InDelta() const { return mMode == ViewMode::Delta; }
		const DeltaSpec&	Delta() const { return mDelta; }			// valid when InDelta()
		ViewMode			HeldMode() const { return mHeldMode; }		// Live or Sample; meaningful in delta
		uint32_t			HeldSample() const { return mHeldSample; }	// meaningful when HeldMode() == Sample

		ViewMode	Mode() const { return mMode; }
		bool		IsLive() const { return mMode == ViewMode::Live; }
		uint32_t	Sample() const { return mSample; }			// meaningful in Sample mode; the held sample in Delta mode
		uint64_t	Epoch() const { return mEpoch; }
		bool		IsEvicted() const;							// Sample mode and Sample() < recorder.FirstRetained(), evaluated now

	private:
		const CellHistoryRecorder&	mRecorder;
		ViewMode	mMode = ViewMode::Live;
		uint32_t	mSample = 0;
		uint64_t	mEpoch = 0;
		DeltaSpec	mDelta;
		ViewMode	mHeldMode = ViewMode::Live;
		uint32_t	mHeldSample = 0;
		void		RestoreHeld();
		bool		mEvictedSeen = false;	// eviction state as of the last epoch bump/Update
	};
}
