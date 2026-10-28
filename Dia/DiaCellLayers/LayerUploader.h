#pragma once

// LayerUploader: per-frame, version-gated upload of the active base layer (and its optional elevation layer) from the
// CellLayerRegistry into the CellField registry, plus the colour-map registration and draw request for the viewer.
// Generic: no game types (CL-005), no singletons (CL-006). Owned and driven by one thread (the viewer's).
//
// Per Update(base, layers):
//  - Version gate: a layer is read + uploaded only when newly shown or when layer.version() changed; otherwise no
//    read and no upload (counted in SkippedFrames when the whole Update did nothing).
//  - Only the base and its elevation layer (schema ui.elevation_layer) hold CellField layers; any other held layer is
//    destroyed when the base changes. At the CellField 32-layer cap CreateLayer can fail until the next Drain frees a
//    destroyed slot: Update then returns CreatePending and retries on the next call (no read is consumed).
//  - Colour maps are registered once per ramp/palette id. Id = StringCRC(name).Value(): the ramp's resolved name
//    (ui.ramp, or the fallback), or the schema full key for a category palette.
//  - Schema ui keys read: ramp, inset, backdrop (#rrggbb), elevation_layer. Schema min/max give rampMin/rampMax;
//    without them the observed min/max are used.
//  - View value source (IViewValueSource, installed on the registry): while one is installed and not live, GetValues
//    replaces the layer's read; false from it gives UploadResult::NoData (held layers destroyed, nothing drawn, never a
//    live fallback). Cached reads are keyed by version() AND ViewEpoch() AND whether they came from the source.
//    A source presentation (ramp id, range) replaces the schema ramp and range of a float base.
// The viewer submits DrawRequest(); this class never calls the renderer.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCore/DebugDraw/CellFieldDrawRequest.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>

#include <stdint.h>

namespace Dia::ValueSchema { class ValueSchemaService; class RampResolver; }

namespace Dia::CellLayers
{
	enum class UploadResult
	{
		Ok,				// layer(s) current (uploaded or version-gated skip)
		NoBase,			// base null; held CellField layers destroyed
		ReadFailed,		// invalid grid, missing read/version delegate, or CellField rejected the update
		CreatePending,	// CellField layer table full (destroy-then-recreate); retry next frame
		NoData			// view source not live and has no data for the base; held CellField layers destroyed (map cleared)
	};

	class LayerUploader
	{
	public:
		// cellField is not owned. schema may be null (grey fallback ramp, observed range).
		LayerUploader(Dia::Graphics::CellFieldRegistry& cellField, const Dia::ValueSchema::ValueSchemaService* schema);
		~LayerUploader();
		LayerUploader(const LayerUploader&) = delete;
		LayerUploader& operator=(const LayerUploader&) = delete;

		UploadResult	Update(const CellLayerDesc* base, const CellLayerRegistry& layers);

		const Dia::Core::CellFieldDrawRequest&	DrawRequest() const { return mRequest; }
		float			ObservedMin() const { return mObservedMin; }
		float			ObservedMax() const { return mObservedMax; }
		// The base layer's values as last uploaded (a private copy, safe to read after the elevation layer was read).
		// Null (count 0) if id is not the uploaded base or its last upload failed / was cleared.
		const float*	BaseValues(Dia::Core::StringCRC id, uint32_t& count) const;

		uint64_t		Reads() const { return mReads; }
		uint64_t		Uploads() const { return mUploads; }
		uint64_t		SkippedFrames() const { return mSkippedFrames; }
		uint64_t		ReadFailures() const { return mReadFailures; }
		uint32_t		RegisteredColourMapCount() const { return mColourMapCount; }

	private:
		struct Slot
		{
			Dia::Core::StringCRC				id;
			const Dia::Graphics::CellGridDesc*	grid = nullptr;
			Dia::Core::CellFieldLayerHandle		handle;
			uint64_t							lastVersion = 0;
			uint64_t							lastEpoch = 0;
			bool								lastFromSource = false;
			bool								held = false;
		};
		static const uint32_t kMaxColourMaps = 256;

		Slot*			FindSlot(Dia::Core::StringCRC id);
		Slot*			FreeSlot();
		void			ReleaseSlot(Slot& slot);
		UploadResult	Sync(Slot& slot, const CellLayerDesc& layer, IViewValueSource* source, bool isBase, bool& didUpload);
		bool			EnsureBuffer(uint32_t count);
		void			StoreBaseValues(Dia::Core::StringCRC id, uint32_t count);
		bool			MarkColourMap(uint32_t id);
		uint32_t		RegisterColourMap(const CellLayerDesc& base, const ViewPresentation* presentation, float& rampMin, float& rampMax);

		Dia::Graphics::CellFieldRegistry&			mCellField;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		Dia::ValueSchema::RampResolver*				mResolver = nullptr;	// owned; null without a schema
		Slot			mSlots[2];
		float*			mBuffer = nullptr;
		uint32_t		mBufferCapacity = 0;
		float*			mBaseCopy = nullptr;
		uint32_t		mBaseCopyCapacity = 0;
		uint32_t		mBaseCopyCount = 0;
		Dia::Core::StringCRC	mBaseCopyId;
		uint32_t		mColourMaps[kMaxColourMaps];
		uint32_t		mColourMapCount = 0;
		Dia::Core::CellFieldDrawRequest	mRequest;
		float			mObservedMin = 0.0f;
		float			mObservedMax = 0.0f;
		uint64_t		mReads = 0;
		uint64_t		mUploads = 0;
		uint64_t		mSkippedFrames = 0;
		uint64_t		mReadFailures = 0;
	};
}
