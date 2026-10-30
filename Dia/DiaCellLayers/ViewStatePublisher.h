#pragma once
#ifdef DIA_DEBUG

// ViewStatePublisher: builds and publishes the "cells.view.state" topic (editor mirror, Feature 5). Runs on the viewer's PU
// (SimPU): call Update() once per frame. Read-only: it only reads the viewer, registry and selection.
//
// Payload: { viewVersion, layersVersion, grid {id, topology, width, height}, base, overlays[], selection {count, version,
// hash (16 hex)}, history (null until DiaCellHistory) }. grid.id is "grid.<index>" (CellGridDesc carries no id; index = the
// grid's position in CellLayerRegistry::GridAt). Empty state: grid null, base "".
//
// Change detection: a cheap state snapshot is compared with the last PUBLISHED snapshot each frame; the JSON is only built on
// publish. Coalescing: publishes are at least kMinPublishIntervalMs apart (10/s). A change inside the interval is held and
// sent by the first Update after the interval ends (trailing publish, so the final state is always delivered); a change
// after idle publishes immediately. viewVersion increases by one on every publish; layersVersion mirrors
// CellLayerRegistry::LayersVersion(). Log channel CEL. Sink and clock are injected and borrowed.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/ITopicSink.h>
#include <DiaCellLayers/SelectionFingerprint.h>

namespace Dia::CellLayers
{
	class ViewStatePublisher
	{
	public:
		static const uint64_t kMinPublishIntervalMs = 100;

		static Dia::Core::StringCRC TopicId() { return Dia::Core::StringCRC("cells.view.state"); }

		ViewStatePublisher(const CellLayerRegistry& registry, const CellLayerViewerDomain& viewer, ITopicSink& sink, const IMonotonicClock& clock);
		ViewStatePublisher(const ViewStatePublisher&) = delete;
		ViewStatePublisher& operator=(const ViewStatePublisher&) = delete;

		void		Update();

		uint32_t	ViewVersion() const { return mViewVersion; }
		uint32_t	PublishCount() const { return mPublishCount; }
		uint32_t	CoalescedCount() const { return mCoalescedCount; }	// Update calls that held a change back

	private:
		struct Snapshot
		{
			uint32_t				layersVersion = 0;
			const Dia::Graphics::CellGridDesc*	grid = nullptr;
			Dia::Core::StringCRC	topology;
			uint16_t				width = 0;
			uint16_t				height = 0;
			Dia::Core::StringCRC	base;
			uint32_t				overlayCount = 0;
			Dia::Core::StringCRC	overlays[CellLayerRegistry::kMaxOverlays];	// enabled overlays, registry order
			SelectionFingerprint	selection;
			bool operator==(const Snapshot& o) const;
			bool operator!=(const Snapshot& o) const { return !(*this == o); }
		};

		void	Capture(Snapshot& out);
		void	Publish(const Snapshot& s, uint64_t now);

		const CellLayerRegistry&		mRegistry;
		const CellLayerViewerDomain&	mViewer;
		ITopicSink&						mSink;
		const IMonotonicClock&			mClock;
		SelectionFingerprintCache		mFingerprint;
		Snapshot						mPublished;
		bool							mHasPublished = false;
		uint64_t						mLastPublishMs = 0;
		uint32_t						mViewVersion = 0;
		uint32_t						mPublishCount = 0;
		uint32_t						mCoalescedCount = 0;
	};
}

#endif // DIA_DEBUG
