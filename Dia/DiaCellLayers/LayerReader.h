#pragma once

// LayerReader: returns the values of any registered layer (not just the active base), as the viewer shows them.
// Calls a layer's read delegate (or the installed IViewValueSource) only when the cache key changes; otherwise the
// cached buffer is returned. Shared by selection, query and aggregates so each layer is read at most once per version.
// Generic: no game types (CL-005), no singletons (CL-006). Owned and used by one processing unit; not thread-safe.
//
// Source rule (identical to LayerUploader::Sync): with no source, or a live source, the layer's own read is used and
// GetValues is never called. With a non-live source GetValues is the only value path; false means "no data in this
// view" (hasData = false), never a fallback to live values.
// Cache key per layer: (version(), source ViewEpoch() or 0, values-came-from-source).
// Re-entrancy: a read delegate may call back into code that reads other layers through this reader; a slot whose buffer is
// being filled is never handed out to the nested read (when every slot is mid-read the nested Read returns no data).
// Returned pointers stay valid until a later Read evicts that layer's slot, or Invalidate/destruction.

#include <DiaCellLayers/CellLayerRegistry.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	struct LayerValues
	{
		const float*	values = nullptr;
		uint32_t		count = 0;
		uint64_t		version = 0;
		bool			hasData = false;
	};

	class LayerReader
	{
	public:
		// registry is borrowed and must outlive the reader. maxSlots is clamped to [1, CellLayerRegistry::kMaxLayers].
		explicit LayerReader(const CellLayerRegistry& registry, uint32_t maxSlots = CellLayerRegistry::kMaxLayers);
		~LayerReader();
		LayerReader(const LayerReader&) = delete;
		LayerReader& operator=(const LayerReader&) = delete;

		LayerValues		Read(Dia::Core::StringCRC layerId);
		void			Invalidate();

		uint32_t		Reads() const { return mReads; }			// actual read / GetValues calls
		uint32_t		CacheHits() const { return mCacheHits; }
		uint32_t		SlotCount() const { return mSlotCount; }

	private:
		struct Slot
		{
			bool									used = false;
			Dia::Core::StringCRC					id;
			const Dia::Graphics::CellGridDesc*		grid = nullptr;
			uint32_t								count = 0;
			uint32_t								capacity = 0;
			float*									buffer = nullptr;
			uint64_t								version = 0;
			uint64_t								epoch = 0;
			bool									fromSource = false;
			bool									hasData = false;
			bool									reading = false;	// a read delegate / source is filling the buffer (re-entrancy guard)
			uint64_t								lastUse = 0;
		};

		Slot*			Acquire(Dia::Core::StringCRC id);

		const CellLayerRegistry&	mRegistry;
		Slot*						mSlots;
		uint32_t					mSlotCount;
		uint64_t					mTick = 0;
		uint32_t					mReads = 0;
		uint32_t					mCacheHits = 0;
	};
}
