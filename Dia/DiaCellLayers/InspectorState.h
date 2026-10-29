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
// SetActive / SetGroupExpanded that change the shown set set pending immediately. A Tick that finds nothing dirty is not a pass.
// A pass reads each dirty layer once through the LayerReader (at most one read per layer version). Dirty rules: selection
// version change -> all shown layers; a layer's own version change -> that layer; newly shown layer -> that layer.
// Empty selection (null or Count() == 0): HasSelection() false, cached stats cleared, nothing computed, no reads, never pending.
// The active base's Histogram (float bases only) is recomputed whenever the base's stats are.
// Pinned-A / freeze (Task 4) are not implemented here. Generic (CL-005), no singletons (CL-006), no STL in the API (PD-004).

#include <DiaCellLayers/SelectionStats.h>

#include <stdint.h>

namespace Dia::CellLayers
{
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

		// Once per frame (see header). Returns true when a recompute pass ran in this call.
		bool Tick(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const ICellSelectionView* selection);

		bool	HasSelection() const { return mHasSelection; }
		bool	IsUpdating() const { return mPending; }

		const LayerStats*	StatsFor(Dia::Core::StringCRC layerId) const;	// null when not shown, not yet computed or no selection
		const Histogram*	BaseHistogram() const { return mHasHistogram ? &mHistogram : nullptr; }
		uint32_t			ShownLayerCount() const { return mShownCount; }
		Dia::Core::StringCRC	ShownLayerAt(uint32_t index) const;	// kZero out of range; index 0 is the active base when set

		uint32_t	PassCount() const { return mPassCount; }
		uint32_t	LastPassLayerCount() const { return mLastPassLayerCount; }

	private:
		struct Entry
		{
			Dia::Core::StringCRC	id;
			bool					computed = false;
			uint64_t				seenVersion = 0;
			LayerStats				stats;
		};

		void	RebuildShown(const CellLayerRegistry& registry);
		void	ClearComputed();
		bool	AnyDirty(const CellLayerRegistry& registry) const;
		bool	NeedsCompute(const CellLayerRegistry& registry, const Entry& e) const;

		Entry*					mEntries;	// kMaxShown, heap (LayerStats is large)
		uint32_t				mShownCount = 0;
		const Dia::Graphics::CellGridDesc*	mGrid = nullptr;
		Dia::Core::StringCRC	mBase;
		Dia::Core::Containers::String32	mActiveGroup;
		Dia::Core::Containers::String32	mExpanded[kMaxGroups];
		uint32_t				mExpandedCount = 0;
		bool					mShownStale = true;	// shown set must be rebuilt (active / expanded changed)
		bool					mPending = false;
		bool					mHasSelection = false;
		bool					mSelSeen = false;
		uint32_t				mSelVersion = 0;
		Histogram				mHistogram;
		bool					mHasHistogram = false;
		uint32_t				mPassCount = 0;
		uint32_t				mLastPassLayerCount = 0;
	};
}
