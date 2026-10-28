#pragma once

// CellLayerRegistry: systems register per-cell "layers" (a value per cell, on a CellGridDesc) and "overlays"
// (draw callbacks); a viewer enumerates them. Generic: no game types (CL-005), no singletons (CL-006).
//
// Owned by one processing unit. Register/Unregister must be called from that PU; a registration from any other
// PU is rejected (Error on log channel CEL). A duplicate id or a registration past capacity is likewise
// rejected, leaving the registry unchanged. Callbacks are FastDelegates (bind with fastdelegate::MakeDelegate).
// Descs hold borrowed pointers (grid); the registered system must outlive its registration.

#include <DiaCellLayers/CellLayersTypes.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Misc/FastDelegate.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Strings/String32.h>
#include <DiaGraphics/CellField/CellGridDesc.h>

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

	// The selection is opaque here; Feature 2 defines CellSelection.
	struct CellOverlayDesc
	{
		Dia::Core::StringCRC					id;
		Dia::Core::Containers::String32			group;
		Dia::Core::Containers::String32			label;
		fastdelegate::FastDelegate2<Dia::Core::IDebugDraw&, const void*>	draw;
	};

	enum class CellRegisterResult
	{
		Ok, Duplicate, CapacityReached, WrongProcessingUnit
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

		CellRegisterResult	Register(const CellLayerDesc& desc);
		bool				Unregister(Dia::Core::StringCRC id);
		const CellLayerDesc*	Find(Dia::Core::StringCRC id) const;
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

	private:
		bool				IsOwnerPu(const char* what, Dia::Core::StringCRC id) const;
		bool				HasDefaultBase(const CellLayerDesc& desc) const;
		bool				IsFirstOfGrid(uint32_t layerIndex) const;

		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		CurrentPuIdFn		mCurrentPuId;
		uint64_t			mOwnerPuId;
		uint32_t			mLayerCount = 0;
		uint32_t			mOverlayCount = 0;
		CellLayerDesc		mLayers[kMaxLayers];
		CellOverlayDesc		mOverlays[kMaxOverlays];
	};
}
