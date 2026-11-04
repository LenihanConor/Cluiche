#ifdef DIA_DEBUG
#include <DiaCellLayers/ViewStatePublisher.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <stdio.h>

namespace Dia::CellLayers
{
	using Dia::Core::StringCRC;

	bool ViewStatePublisher::Snapshot::operator==(const Snapshot& o) const
	{
		if (layersVersion != o.layersVersion || grid != o.grid || topology != o.topology || width != o.width || height != o.height
			|| base != o.base || overlayCount != o.overlayCount || selection != o.selection || historyStamp != o.historyStamp)
		{
			return false;
		}
		for (uint32_t i = 0; i < overlayCount; ++i)
		{
			if (overlays[i] != o.overlays[i]) { return false; }
		}
		return true;
	}

	ViewStatePublisher::ViewStatePublisher(const CellLayerRegistry& registry, const CellLayerViewerDomain& viewer, ITopicSink& sink, const IMonotonicClock& clock)
		: mRegistry(registry), mViewer(viewer), mSink(sink), mClock(clock)
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		mMetricPublishes = metrics.RegisterCounter(StringCRC("dia.celllayers.mirror.topic_publishes"));
		mMetricCoalesced = metrics.RegisterCounter(StringCRC("dia.celllayers.mirror.coalesced_updates"));
	}

	void ViewStatePublisher::Capture(Snapshot& out)
	{
		out.layersVersion = mRegistry.LayersVersion();
		const Dia::Graphics::CellGridDesc* grid = mViewer.ActiveGrid();
		out.grid = grid;
		if (grid != nullptr)
		{
			out.topology = grid->topology;
			out.width = grid->width;
			out.height = grid->height;
		}
		out.base = mViewer.ActiveBase();
		out.overlayCount = 0;
		for (uint32_t i = 0; i < mRegistry.OverlayCount(); ++i)
		{
			const CellOverlayDesc* o = mRegistry.OverlayAt(i);
			if (o != nullptr && mViewer.IsOverlayEnabled(o->id)) { out.overlays[out.overlayCount++] = o->id; }
		}
		const ICellSelectionView* sel = mViewer.ActiveSelectionView();
		if (sel != nullptr) { out.selection = mFingerprint.Get(*sel); }
		else { out.selection = SelectionFingerprint(); mFingerprint.Invalidate(); }
			out.historyStamp = mHistory ? mHistory->HistoryStamp() : 0;
	}

	void ViewStatePublisher::Update()
	{
		const unsigned int subscribers = mSink.SubscriberCount(TopicId());
		if (subscribers > 0 && mPrevSubscribers == 0) { DIA_LOG_INFO("CEL", "cells.view.state: first subscriber"); }
		else if (subscribers == 0 && mPrevSubscribers > 0) { DIA_LOG_INFO("CEL", "cells.view.state: last subscriber left"); }
		mPrevSubscribers = subscribers;

		Snapshot current;
		Capture(current);
		if (mHasPublished && current == mPublished) { return; }

		const uint64_t now = mClock.NowMs();
		if (mHasPublished && now - mLastPublishMs < kMinPublishIntervalMs)
		{
			++mCoalescedCount;
			if (mMetricCoalesced) { mMetricCoalesced->Inc(); }	// held; the first Update after the interval publishes the latest state
			return;
		}
		Publish(current, now);
	}

	void ViewStatePublisher::Publish(const Snapshot& s, uint64_t now)
	{
		DIA_PROFILE_SCOPE("celllayers.viewstate.build", ::Dia::Observation::Profile::Category::kDiaGraphics);
		++mViewVersion;

		Json::Value p(Json::objectValue);
		p["viewVersion"] = mViewVersion;
		p["layersVersion"] = s.layersVersion;
		if (s.grid != nullptr)
		{
			char id[MirrorGridId::kBufferSize];
			MirrorGridId::Format(mRegistry, s.grid, id);
			Json::Value g(Json::objectValue);
			g["id"] = id;
			g["topology"] = s.topology.AsChar();
			g["width"] = s.width;
			g["height"] = s.height;
			p["grid"] = g;
		}
		else
		{
			p["grid"] = Json::Value(Json::nullValue);
		}
		p["base"] = s.base != StringCRC::kZero ? s.base.AsChar() : "";
		Json::Value overlays(Json::arrayValue);
		for (uint32_t i = 0; i < s.overlayCount; ++i) { overlays.append(s.overlays[i].AsChar()); }
		p["overlays"] = overlays;
		char hex[SelectionFingerprintFn::kHexLength + 1];
		SelectionFingerprintFn::ToHex(s.selection.mHash, hex);
		Json::Value sel(Json::objectValue);
		sel["count"] = s.selection.mCount;
		sel["version"] = s.selection.mVersion;
		sel["hash"] = hex;
		p["selection"] = sel;
		p["history"] = mHistory ? mHistory->HistoryField() : Json::Value(Json::nullValue);

		mSink.Publish(TopicId(), p);
		mPublished = s;
		mHasPublished = true;
		mLastPublishMs = now;
		++mPublishCount;
		if (mMetricPublishes) { mMetricPublishes->Inc(); }
		DIA_LOG_DEBUG("CEL", "cells.view.state published (viewVersion %u, base '%s', selection %u)", mViewVersion, s.base != StringCRC::kZero ? s.base.AsChar() : "", s.selection.mCount);
	}
}
#endif
