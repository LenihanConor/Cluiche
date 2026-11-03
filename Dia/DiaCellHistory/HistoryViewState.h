#pragma once

// HistoryViewState: which recorded sample (if any) the map is showing. Live follows the simulation; Sample(n) is scrub
// mode, pinned to recorded sample n until GoLive(). Viewing the latest sample is scrub mode, NOT live.
// Read-only over the recorder. Same-PU contract as the recorder. No singletons.
// Epoch() changes whenever the shown view changes (mode, sample, or the viewed sample falling out of the per-cell
// window); Update() must be called once per frame (or before reading) to detect the window passing the viewed sample.
// ViewMode is an enum so a third mode (Delta, Feature 4) can be added without breaking callers.

#include <cstdint>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;

	enum class ViewMode : uint8_t { Live, Sample };

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
		bool Update();

		ViewMode	Mode() const { return mMode; }
		bool		IsLive() const { return mMode == ViewMode::Live; }
		uint32_t	Sample() const { return mSample; }			// meaningful in Sample mode
		uint64_t	Epoch() const { return mEpoch; }
		bool		IsEvicted() const;							// Sample mode and Sample() < recorder.FirstRetained(), evaluated now

	private:
		const CellHistoryRecorder&	mRecorder;
		ViewMode	mMode = ViewMode::Live;
		uint32_t	mSample = 0;
		uint64_t	mEpoch = 0;
		bool		mEvictedSeen = false;	// eviction state as of the last epoch bump/Update
	};
}
