#include <DiaCellHistory/TimelineCardState.h>

#include <DiaCellHistory/CellHistoryRecorder.h>

#include <string.h>

namespace Dia::CellHistory
{
	Json::Value TimelineCardState::HistoryField(const CellHistoryRecorder* recorder, const HistoryViewState* view)
	{
		if (recorder == nullptr || view == nullptr) { return Json::Value(Json::nullValue); }
		const HistoryViewStatus s = DescribeHistoryView(*recorder, *view);
		Json::Value h(Json::objectValue);
		if (view->InDelta())
		{
			char text[80];
			h["mode"] = "delta";
			FormatSide(view->Delta().a, text, sizeof(text));
			h["a"] = text;
			FormatSide(view->Delta().b, text, sizeof(text));
			h["b"] = text;
			h["scale"] = view->Delta().autoScale ? Json::Value(Json::nullValue) : Json::Value(static_cast<double>(view->Delta().fixedScale));
			h["sample"] = Json::Value(Json::nullValue);
			h["label"] = Json::Value(Json::nullValue);
		}
		else if (s.mMode == ViewMode::Sample)
		{
			h["mode"] = "sample";
			h["sample"] = s.mSample;
			h["label"] = s.mLabel[0] != '\0' ? Json::Value(s.mLabel) : Json::Value(Json::nullValue);
		}
		else
		{
			h["mode"] = "live";
			h["sample"] = Json::Value(Json::nullValue);
			h["label"] = Json::Value(Json::nullValue);
		}
		return h;
	}

	bool TimelineCardState::Update(const CellHistoryRecorder& recorder, const HistoryViewState& view, Dia::Core::StringCRC baseLayer,
		const Dia::ValueSchema::ValueSchemaService* schema, const HistoryConfig* config, uint32_t bucketCount)
	{
		const HistoryViewStatus status = DescribeHistoryView(recorder, view);
		const LayerViewState layerState = DescribeLayerView(recorder, view, baseLayer);
		const bool timelineStale = mTimeline.NeedsRebuild(recorder, view, baseLayer);

		if (mBuilt && !timelineStale && layerState == mStampLayerState && status.mFirstRetained == mStampFirstRetained
			&& strcmp(status.mLabel, mStampLabel) == 0)
		{
			return false;
		}

		if (timelineStale || !mBuilt || status.mFirstRetained != mStampFirstRetained)
		{
			mTimeline.Build(recorder, view, baseLayer, schema, config, bucketCount);
		}

		Json::Value card(Json::objectValue);
		card["available"] = true;
		card["baseLayer"] = baseLayer != Dia::Core::StringCRC::kZero ? baseLayer.AsChar() : "";

		Json::Value banner(Json::objectValue);
		banner["visible"] = status.mMode != ViewMode::Live;
		banner["state"] = LayerViewStateName(layerState);
		banner["layer"] = card["baseLayer"];
		banner["sample"] = (status.mMode == ViewMode::Sample) ? Json::Value(status.mSample) : Json::Value(Json::nullValue);
		banner["label"] = status.mLabel;
		banner["firstRetained"] = status.mFirstRetained;
		card["banner"] = banner;

		card["history"] = HistoryField(&recorder, &view);
		card["timeline"] = mTimeline.ToJson();
		mCard = card;

		mBuilt = true;
		++mCardBuilds;
		mStampLayerState = layerState;
		mStampFirstRetained = status.mFirstRetained;
		memcpy(mStampLabel, status.mLabel, sizeof(mStampLabel));
		return true;
	}
}
