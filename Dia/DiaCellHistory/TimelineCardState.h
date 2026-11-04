#pragma once

// TimelineCardState: the typed model behind the history timeline card and the viewer's "not live" banner (Feature 2).
// Pure data, no UI: it owns a TimelineModel and caches the card JSON, rebuilding only when something that changes the
// card changed (new sample, view change, base layer change, layer view state, per-cell window start, status label).
// Same-PU contract as the recorder (SimPU). No singletons; everything is borrowed per call.
//
// Card JSON (the panel renders this and nothing else):
//   { "available": true,
//     "baseLayer": "<id>",
//     "banner": { "visible": bool,                 // true whenever the view is not live
//                 "state": "live"|"data"|"untracked"|"evicted",   // LayerViewStateName for the base layer
//                 "layer": "<id>", "sample": <n or null>, "label": "<text>", "firstRetained": <n> },
//     "history": <HistoryField()>,
//     "timeline": <TimelineModel::ToJson()> }
//
// HistoryField (the future cells.view.state.history value) is null when no source is installed, otherwise
// { "mode": "live"|"sample", "sample": <index or null>, "label": "<text or null>" }. "delta" is reserved (Feature 4).

#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/HistoryViewStatus.h>
#include <DiaCellHistory/TimelineModel.h>
#include <DiaCore/CRC/StringCRC.h>
#include <json/json.h>

namespace Dia::CellHistory
{
	class TimelineCardState
	{
	public:
		// The history value of cells.view.state: null when recorder or view is null.
		static Json::Value HistoryField(const CellHistoryRecorder* recorder, const HistoryViewState* view);

		// Rebuilds the cached card when its inputs changed. Returns true when it rebuilt.
		bool Update(const CellHistoryRecorder& recorder, const HistoryViewState& view, Dia::Core::StringCRC baseLayer,
			const Dia::ValueSchema::ValueSchemaService* schema, const HistoryConfig* config,
			uint32_t bucketCount = TimelineModel::kDefaultBucketCount);

		const Json::Value&	Card() const { return mCard; }	// null until the first Update
		uint64_t			CardBuildCount() const { return mCardBuilds; }
		const TimelineModel& Timeline() const { return mTimeline; }

	private:
		TimelineModel		mTimeline;
		Json::Value			mCard;
		bool				mBuilt = false;
		uint64_t			mCardBuilds = 0;
		LayerViewState		mStampLayerState = LayerViewState::Live;
		uint32_t			mStampFirstRetained = 0;
		char				mStampLabel[HistoryViewStatus::kLabelChars] = {};
	};
}
