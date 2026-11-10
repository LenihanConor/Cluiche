#pragma once

// HistoryViewStatus: a read-only, machine-readable description of what the history view is showing, for the viewer UI
// (the "Viewing sample N" banner / hover badge, the untracked and evicted notices) and the later cells.view.state.history
// field. Pure functions over the recorder and view state; same-PU contract as the recorder. No singletons.
//
// DescribeLayerView answers "what does this layer show in the current view":
//   Live      - not scrubbing: the layer's own live values.
//   HasData   - scrubbing and the recorder holds per-cell values for the viewed sample.
//   Untracked - scrubbing, the layer has no per-cell history (the banner offers Track-this-layer). Takes precedence
//               over Evicted because tracking is the actionable fix.
//   Evicted   - scrubbing, the layer is tracked but the viewed sample is before the per-cell window start.
// Untracked and Evicted both mean "no data" (never live values); HistoryValueSource::GetValues returns false for both.

#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;

	enum class LayerViewState : uint8_t { Live, HasData, Untracked, Evicted };

	struct HistoryViewStatus
	{
		static constexpr uint32_t kLabelChars = 32;
		ViewMode	mMode = ViewMode::Live;
		uint32_t	mSample = 0;			// the viewed sample; while live, the latest recorded sample (0 when none)
		bool		mHasSample = false;		// false while live with nothing recorded
		char		mLabel[kLabelChars] = {};
		bool		mEvicted = false;		// scrubbing and the per-cell window no longer holds the viewed sample
		uint32_t	mFirstRetained = 0;		// per-cell window start
	};

	HistoryViewStatus	DescribeHistoryView(const CellHistoryRecorder& recorder, const HistoryViewState& view);
	LayerViewState		DescribeLayerView(const CellHistoryRecorder& recorder, const HistoryViewState& view, Dia::Core::StringCRC layer);
	const char*			LayerViewStateName(LayerViewState state);	// "live", "data", "untracked", "evicted"
}
