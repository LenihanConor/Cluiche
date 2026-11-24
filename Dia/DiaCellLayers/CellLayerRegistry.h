#pragma once

// CellLayerRegistry: systems register per-cell "layers" (a value per cell, on a CellGridDesc) and "overlays"
// (draw callbacks); a viewer enumerates them. Generic: no game types (CL-005), no singletons (CL-006).
//
// Owned by one processing unit. Register/Unregister must be called from that PU; a registration from any other
// PU is rejected (Error on log channel CEL). A duplicate id or a registration past capacity is likewise
// rejected, leaving the registry unchanged. Callbacks are FastDelegates (bind with fastdelegate::MakeDelegate).
// Descs hold borrowed pointers (grid); the registered system must outlive its registration.

#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCellLayers/CellLayersTypes.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/IViewValueSource.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Misc/FastDelegate.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Strings/String32.h>
#include <DiaGraphics/CellField/CellGridDesc.h>

#include <atomic>
#include <stdint.h>

namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellLayers
{
	struct CellLayerDesc
	{
		Dia::Core::StringCRC					id;
		Dia::Core::StringCRC					schemaKey;
		Dia::Core::Containers::String32			group;
		CellValueKind							kind = CellValueKind::Float;
		const Dia::Graphics::CellGridDesc*		grid = nullptr;		// not owned
		fastdelegate::FastDelegate2<float*, uint32_t>	read;		// copies values into the viewer buffer
		fastdelegate::FastDelegate0<uint64_t>	version;			// change counter
	};

	// draw receives the viewer's active selection as a `const ICellSelectionView*` (null when there is no active grid).
	struct CellOverlayDesc
	{
		Dia::Core::StringCRC					id;
		Dia::Core::Containers::String32			group;
		Dia::Core::Containers::String32			label;
		fastdelegate::FastDelegate2<Dia::Core::IDebugDraw&, const void*>	draw;
	};

	enum class CellRegisterResult
	{
		Ok, Duplicate, CapacityReached, WrongProcessingUnit, NullGrid
	};

	class CellLayerRegistry
	{
	public:
		static const uint32_t kMaxLayers = 128;
		static const uint32_t kMaxOverlays = 64;

		// Returns an id for the calling processing unit; the registry's owner is the id returned at construction.
		// Default (null): a hash of the calling thread id.
		typedef uint64_t (*CurrentPuIdFn)();

		// schema may be null (no schema: first registered layer of a grid is its default base). Not owned.
		explicit CellLayerRegistry(const Dia::ValueSchema::ValueSchemaService* schema = nullptr, CurrentPuIdFn currentPuId = nullptr);
		~CellLayerRegistry();
		CellLayerRegistry(const CellLayerRegistry&) = delete;
		CellLayerRegistry& operator=(const CellLayerRegistry&) = delete;

		// Late attach (CellLayersModule resolves the schema stream after start). Layers keep no cached schema state, so
		// DefaultBase reflects the new schema immediately. Null detaches. Not owned.
		void				SetSchema(const Dia::ValueSchema::ValueSchemaService* schema) { mSchema = schema; }

		// Bumped by every successful Register / Unregister / RegisterOverlay / UnregisterOverlay and by nothing else (not by
		// selection or base changes). Editor mirror: published as layersVersion.
		uint32_t			LayersVersion() const { return mLayersVersion; }

		CellRegisterResult	Register(const CellLayerDesc& desc);
		bool				Unregister(Dia::Core::StringCRC id);
		const CellLayerDesc*	Find(Dia::Core::StringCRC id) const;
		// Registrations rejected since construction (duplicate, capacity, wrong PU; layers and overlays). Thread-safe:
		// wrong-PU rejections come from other threads.
		uint32_t			RejectedRegistrations() const { return mRejected.load(std::memory_order_relaxed); }
		uint32_t			LayerCount() const { return mLayerCount; }
		const CellLayerDesc*	LayerAt(uint32_t index) const;	// registration order; null out of range

		CellRegisterResult	RegisterOverlay(const CellOverlayDesc& desc);
		bool				UnregisterOverlay(Dia::Core::StringCRC id);
		const CellOverlayDesc*	FindOverlay(Dia::Core::StringCRC id) const;
		uint32_t			OverlayCount() const { return mOverlayCount; }
		const CellOverlayDesc*	OverlayAt(uint32_t index) const;

		// Grids with at least one layer, in order of first registration.
		uint32_t			GridCount() const;
		const Dia::Graphics::CellGridDesc*	GridAt(uint32_t index) const;
		bool				IsMultiGrid() const { return GridCount() > 1; }
		uint32_t			LayerCountForGrid(const Dia::Graphics::CellGridDesc* grid) const;
		const CellLayerDesc*	LayerOfGrid(const Dia::Graphics::CellGridDesc* grid, uint32_t index) const;
		// First layer of the grid whose schema has ui.default_base: true, else its first registered layer.
		const CellLayerDesc*	DefaultBase(const Dia::Graphics::CellGridDesc* grid) const;

		// View value source (see IViewValueSource.h). At most one; a second install is rejected (Error on CEL) and
		// returns false, as does null. Borrowed: the source must outlive its installation. Not thread-safe against
		// install/clear; install from the registry's PU during setup.
		bool				SetViewValueSource(IViewValueSource* source);
		void				ClearViewValueSource() { mViewSource = nullptr; }
		IViewValueSource*	ViewValueSource() const { return mViewSource; }
		// True when no source is installed or the source is live. Systems must refuse edits while false.
		bool				IsLive() const { return mViewSource == nullptr || mViewSource->IsLive(); }

		// Per-grid selection store (Selection feature). One CellSelection per grid, created lazily by SelectionFor, at most
		// kMaxSelectionGrids grids; beyond that SelectionFor returns null (Warning on CEL, once). Both accessors Reset(grid)
		// first, so a grid whose width/height/topology changed is cleared (Info on CEL when that dropped a non-empty
		// selection). Entries for grids with no registered layer are kept but SelectionView returns null for them (never
		// stale data). Call from the registry's PU, like Register.
		static const uint32_t kMaxSelectionGrids = 8;
		CellSelection*		SelectionFor(const Dia::Graphics::CellGridDesc* grid);
		// Null when the grid is null, has no registered layer, or has no selection entry yet.
		const ICellSelectionView*	SelectionView(const Dia::Graphics::CellGridDesc* grid) const;

		// Selection Inspector detail providers (CL-006: reached through the registry, not a static). Same owner PU as the layers.
		CellDetailRegistry&			Details() { return mDetails; }
		const CellDetailRegistry&	Details() const { return mDetails; }

	private:
		struct SelectionEntry
		{
			const Dia::Graphics::CellGridDesc*	grid = nullptr;
			CellSelection						selection;
		};
		SelectionEntry*		FindSelection(const Dia::Graphics::CellGridDesc* grid) const;
		static void			ResetEntry(SelectionEntry& e);

		bool				IsOwnerPu(const char* what, Dia::Core::StringCRC id) const;
		void				Reject() { mRejected.fetch_add(1, std::memory_order_relaxed); }
		bool				HasDefaultBase(const CellLayerDesc& desc) const;
		bool				IsFirstOfGrid(uint32_t layerIndex) const;

		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		CurrentPuIdFn		mCurrentPuId;
		uint64_t			mOwnerPuId;
		uint32_t			mLayerCount = 0;
		uint32_t			mOverlayCount = 0;
		uint32_t			mLayersVersion = 0;
		std::atomic<uint32_t>	mRejected{ 0 };
		IViewValueSource*	mViewSource = nullptr;
		CellLayerDesc		mLayers[kMaxLayers];
		CellOverlayDesc		mOverlays[kMaxOverlays];
		mutable SelectionEntry	mSelections[kMaxSelectionGrids];
		bool				mSelectionCapacityLogged = false;
		CellDetailRegistry	mDetails;
	};
}
