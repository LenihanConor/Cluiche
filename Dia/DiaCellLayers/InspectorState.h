#pragma once

// InspectorState: the Selection Inspector's compute state (Feature 4, no UI). Decides WHICH layers get aggregates and WHEN,
// and caches the results for the card. Owned by the viewer domain; the registry / reader / schema are passed by reference
// each Tick (CL-006). Runs on one processing unit (the viewer's SimPU): not thread-safe, no locking.
//
// Shown set = the active base layer (always, even when its group is collapsed) + every layer of the ACTIVE GRID whose group is
// expanded. Layers of other grids and layers in collapsed groups are never read. Capacity: kMaxShown layers (the base first,
// then registration order); layers beyond it are not shown. Expanded groups: kMaxGroups names (further expands are ignored).
// The active base's group is expanded by default: when the active grid or the active base's group changes, the expanded set
// resets to just that group (a base change inside the same group keeps the user's expanded set).
//
// Frame semantics (the card shows an "updating" mark, never stale numbers without it). Tick() is called once per frame:
//   - a change is DETECTED in a Tick (selection Version(), a shown layer's version(), shown set / active base / expanded set)
//     and sets pending: IsUpdating() is true after that Tick, i.e. through the rest of that frame;
//   - the NEXT Tick runs ONE pass (recomputes every dirty shown layer once, clears pending, returns true). If the inputs changed
//     again meanwhile, that same pass covers them (no starvation while a selection is being dragged).
// A registry change (a layer registered or unregistered, CellLayerRegistry::LayersVersion) is detected in Tick and rebuilds the
// shown set, so a layer added later to the active grid's expanded group appears and a removed one disappears.
// SetActive / SetGroupExpanded that change the shown set set pending immediately. A Tick that finds nothing dirty is not a pass.
// A pass reads each dirty layer once through the LayerReader (at most one read per layer version). Dirty rules: selection
// version change -> all shown layers; a layer's own version change -> that layer; newly shown layer -> that layer.
// Empty selection (null or Count() == 0): HasSelection() false, cached stats cleared, nothing computed, no reads, never pending.
// The active base's Histogram (float bases only) is recomputed whenever the base's stats are.
//
// Pin / compare: Pin() copies the current selection's cell set as A (own bitset sized to the grid; null / empty selection or a
// grid other than the active one -> false, no change; re-pinning replaces A). A is LIVE by default: A's stats are computed from
// A's cell set for the same shown layers, in the same pass as B's, and recomputed when that layer's version changes (A does not
// depend on B's selection version). PinFreeze() additionally computes and stores A's stats for EVERY registered layer of the grid
// once, synchronously (one explicit cost; each layer is read at most once per version through the LayerReader). Frozen stats are
// never recomputed. StatsForA() returns the live or frozen stats of a layer (null when not available); FrozenAvailable() is false
// for a layer registered after the freeze (the card shows "not frozen"). Unpin() clears A. A is also cleared when the active grid
// changes or the grid's pointer, size or topology id changes (checked in SetActive and every Tick), like the selection.
// ComputeDelta() gives B minus A for count, min, mean and max. Generic (CL-005), no singletons (CL-006), no STL in the API (PD-004).

#include <DiaCellLayers/SelectionStats.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	// B minus A. countDelta is always valid; min / mean / max deltas are valid only when hasRange (both sides have a range).
	struct LayerDelta
	{
		int64_t	countDelta = 0;
		bool	hasRange = false;
		double	minDelta = 0.0;
		double	meanDelta = 0.0;
		double	maxDelta = 0.0;
	};
	void ComputeDelta(const LayerStats& b, const LayerStats& a, LayerDelta& out);

	class InspectorState
	{
	public:
		static constexpr uint32_t kMaxShown = 32;
		static constexpr uint32_t kMaxGroups = 16;

		InspectorState();
		~InspectorState();
		InspectorState(const InspectorState&) = delete;
		InspectorState& operator=(const InspectorState&) = delete;

		// Sets the active grid and base layer (null / kZero = none). No-op when unchanged. A grid change clears cached stats.
		void SetActive(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, Dia::Core::StringCRC baseLayerId);

		void SetGroupExpanded(const char* group, bool expanded);
		bool IsGroupExpanded(const char* group) const;
		uint32_t	ExpandedGroupCount() const { return mExpandedCount; }
		const char*	ExpandedGroupAt(uint32_t index) const { return index < mExpandedCount ? mExpanded[index].AsCStr() : ""; }

		// Once per frame (see header). Returns true when a recompute pass ran in this call.
		bool Tick(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const ICellSelectionView* selection);

		bool	HasSelection() const { return mHasSelection; }
		bool	IsUpdating() const { return mPending; }

		const LayerStats*	StatsFor(Dia::Core::StringCRC layerId) const;	// null when not shown, not yet computed or no selection
		const Histogram*	BaseHistogram() const { return mHasHistogram ? &mHistogram : nullptr; }
		const Dia::Graphics::CellGridDesc*	ActiveGrid() const { return mGrid; }
		uint32_t			ShownLayerCount() const { return mShownCount; }
		Dia::Core::StringCRC	ShownLayerAt(uint32_t index) const;	// kZero out of range; index 0 is the active base when set

		// Pin / compare (see header). Pin / PinFreeze return false and change nothing for a null / empty selection, a null grid or a
		// grid that is not the active one. PinFreeze reads every registered layer of the grid once (synchronously).
		bool	Pin(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, const ICellSelectionView* selection);
		bool	PinFreeze(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const Dia::Graphics::CellGridDesc* grid, const ICellSelectionView* selection);
		void	Unpin();
		bool	IsPinned() const { return mPinned != nullptr; }
		bool	IsFrozen() const { return mPinned != nullptr && mFrozen != nullptr; }
		uint32_t	PinnedCount() const;
		uint32_t	FrozenLayerCount() const { return mFrozenCount; }	// layers stored by PinFreeze (0 when live)
		const LayerStats*	StatsForA(Dia::Core::StringCRC layerId) const;	// live or frozen A; null when not pinned / not available
		bool				FrozenAvailable(Dia::Core::StringCRC layerId) const;	// frozen A holds stats for this layer

		// True while a recompute pass or a freeze is running. A layer read delegate may call back into the domain during one;
		// the state-changing calls (SetActive, Pin, PinFreeze, Unpin, Tick) are then refused (false / no-op) so the pinned set,
		// frozen array and entries the pass is reading are never freed or rebuilt under it.
		bool		IsBusy() const { return mBusy; }

		// 0 = live / no source installed; else ViewEpoch()+1. Cache keys over viewed values must include it.
		static uint64_t	SourceKey(const CellLayerRegistry& registry);	// 0 = live / no source; else ViewEpoch()+1

		uint32_t	PassCount() const { return mPassCount; }
		uint32_t	LastPassLayerCount() const { return mLastPassLayerCount; }
		double		LastPassMs() const { return mLastPassMs; }

	private:
		struct Entry
		{
			Dia::Core::StringCRC	id;
			bool					computed = false;
			uint64_t				seenVersion = 0;
			uint64_t				seenSource = 0;		// SourceKey() at compute time
			LayerStats				stats;
			bool					aComputed = false;	// live A
			uint64_t				aSeenVersion = 0;
			uint64_t				aSeenSource = 0;
			LayerStats				aStats;
		};
		struct FrozenEntry
		{
			Dia::Core::StringCRC	id;
			LayerStats				stats;
		};
		class PinnedSet;

		void	RebuildShown(const CellLayerRegistry& registry);
		void	ClearComputed();
		bool	AnyDirty(const CellLayerRegistry& registry) const;
		bool	NeedsCompute(const CellLayerRegistry& registry, const Entry& e) const;
		bool	NeedsComputeA(const CellLayerRegistry& registry, const Entry& e) const;
		void	CheckPinIdentity();
		void	WarnUnresolvedSchemaKey(const Dia::ValueSchema::ValueSchemaService* schema, const CellLayerDesc* layer);

		Entry*					mEntries;	// kMaxShown, heap (LayerStats is large)
		uint32_t				mShownCount = 0;
		const Dia::Graphics::CellGridDesc*	mGrid = nullptr;
		Dia::Core::StringCRC	mBase;
		Dia::Core::Containers::String32	mActiveGroup;
		Dia::Core::Containers::String32	mExpanded[kMaxGroups];
		uint32_t				mExpandedCount = 0;
		uint32_t				mRegistryVersion = 0;	// registry LayersVersion() at the last RebuildShown
		bool					mBusy = false;
		bool					mShownStale = true;	// shown set must be rebuilt (active / expanded changed)
		bool					mPending = false;
		bool					mHasSelection = false;
		bool					mSelSeen = false;
		uint32_t				mSelVersion = 0;
		Histogram				mHistogram;
		bool					mHasHistogram = false;
		uint32_t				mPassCount = 0;
		uint32_t				mLastPassLayerCount = 0;
		double					mLastPassMs = 0.0;
		PinnedSet*				mPinned = nullptr;
		FrozenEntry*			mFrozen = nullptr;	// frozen A stats, one per layer of the grid at freeze time
		uint32_t				mFrozenCount = 0;
		void*					mMetricPasses = nullptr;		// Counter
		void*					mMetricLayersPerPass = nullptr;	// Gauge
		void*					mMetricPassMs = nullptr;		// Gauge
		void*					mMetricPinned = nullptr;		// Gauge (0/1)
		static constexpr uint32_t kMaxWarnedSchemaKeys = 16;
		Dia::Core::StringCRC	mWarnedSchemaKeys[kMaxWarnedSchemaKeys];
		uint32_t				mWarnedSchemaKeyCount = 0;
	};
}
