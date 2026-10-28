#include <DiaCellLayers/LayerUploader.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaValueSchema/RampResolver.h>

#include <math.h>
#include <string.h>

namespace Dia::CellLayers
{
	using Dia::Core::StringCRC;
	using Dia::Graphics::CellFieldRegistry;

	namespace
	{
		const char* const kFallbackRampName = "sequential.grey";
		const char* const kDefaultPaletteName = "celllayers.default_palette";

		bool GridValid(const Dia::Graphics::CellGridDesc* g) { return g != nullptr && g->width > 0 && g->height > 0; }

		bool LayerReadable(const CellLayerDesc& l)
		{
			return GridValid(l.grid) && !l.read.empty() && !l.version.empty();
		}

		const char* UiString(const Dia::ValueSchema::ValueInfo* info, const char* key)
		{
			if (info == nullptr || !info->mUi.isObject()) { return nullptr; }
			const Json::Value& v = info->mUi[key];
			return v.isString() ? v.asCString() : nullptr;
		}
	}

	LayerUploader::LayerUploader(CellFieldRegistry& cellField, const Dia::ValueSchema::ValueSchemaService* schema)
		: mCellField(cellField)
		, mSchema(schema)
	{
		if (mSchema != nullptr) { mResolver = new Dia::ValueSchema::RampResolver(*mSchema); }
	}

	LayerUploader::~LayerUploader()
	{
		for (Slot& s : mSlots) { ReleaseSlot(s); }
		delete mResolver;
		delete[] mBuffer;
		delete[] mBaseCopy;
	}

	const float* LayerUploader::BaseValues(StringCRC id, uint32_t& count) const
	{
		if (mBaseCopyCount == 0 || mBaseCopyId != id) { count = 0; return nullptr; }
		count = mBaseCopyCount;
		return mBaseCopy;
	}

	void LayerUploader::StoreBaseValues(StringCRC id, uint32_t count)
	{
		if (count > mBaseCopyCapacity)
		{
			float* grown = new float[count];
			delete[] mBaseCopy;
			mBaseCopy = grown;
			mBaseCopyCapacity = count;
		}
		memcpy(mBaseCopy, mBuffer, sizeof(float) * count);
		mBaseCopyCount = count;
		mBaseCopyId = id;
	}

	LayerUploader::Slot* LayerUploader::FindSlot(StringCRC id)
	{
		for (Slot& s : mSlots) { if (s.held && s.id == id) { return &s; } }
		return nullptr;
	}

	LayerUploader::Slot* LayerUploader::FreeSlot()
	{
		for (Slot& s : mSlots) { if (!s.held) { return &s; } }
		return nullptr;
	}

	void LayerUploader::ReleaseSlot(Slot& slot)
	{
		if (slot.held) { mCellField.Destroy(slot.handle); }
		slot = Slot();
	}

	bool LayerUploader::EnsureBuffer(uint32_t count)
	{
		if (count <= mBufferCapacity) { return true; }
		float* grown = new float[count];
		delete[] mBuffer;
		mBuffer = grown;
		mBufferCapacity = count;
		return true;
	}

	bool LayerUploader::MarkColourMap(uint32_t id)
	{
		for (uint32_t i = 0; i < mColourMapCount; ++i) { if (mColourMaps[i] == id) { return false; } }
		if (mColourMapCount < kMaxColourMaps) { mColourMaps[mColourMapCount++] = id; }
		return true;
	}

	UploadResult LayerUploader::Sync(Slot& slot, const CellLayerDesc& layer, bool isBase, bool& didUpload)
	{
		if (!LayerReadable(layer))
		{
			++mReadFailures;
			DIA_LOG_ERROR("CEL", "LayerUploader: layer 0x%x has no valid grid or read/version delegate", layer.id.Value());
			ReleaseSlot(slot);
			return UploadResult::ReadFailed;
		}
		if (slot.held && slot.grid != layer.grid) { ReleaseSlot(slot); }

		if (!slot.held)
		{
			Dia::Core::CellFieldLayerHandle handle;
			const auto kind = layer.kind == CellValueKind::Category ? Dia::Graphics::CellFieldValueKind::Category : Dia::Graphics::CellFieldValueKind::Float;
			const Dia::Graphics::CellFieldResult r = mCellField.CreateLayer(*layer.grid, kind, handle);
			if (!r.ok)
			{
				if (r.error == CellFieldRegistry::kErrMaxLayers) { return UploadResult::CreatePending; }
				++mReadFailures;
				DIA_LOG_ERROR("CEL", "LayerUploader: CellField create failed: %s", r.error ? r.error : "unknown");
				return UploadResult::ReadFailed;
			}
			slot.id = layer.id;
			slot.grid = layer.grid;
			slot.handle = handle;
			slot.held = true;
			slot.lastVersion = 0;
		}
		else if (slot.lastVersion == layer.version())
		{
			return UploadResult::Ok;	// version gate: no read, no upload
		}

		const uint64_t version = layer.version();
		const uint32_t count = static_cast<uint32_t>(layer.grid->width) * layer.grid->height;
		EnsureBuffer(count);
		layer.read(mBuffer, count);
		++mReads;

		if (isBase)
		{
			bool any = false;
			float mn = 0.0f, mx = 0.0f;
			for (uint32_t i = 0; i < count; ++i)
			{
				const float v = mBuffer[i];
				if (!(v == v) || v > 3.4e38f || v < -3.4e38f) { continue; }
				if (!any) { mn = mx = v; any = true; }
				else { if (v < mn) { mn = v; } if (v > mx) { mx = v; } }
			}
			mObservedMin = mn;
			mObservedMax = mx;
		}

		const Dia::Graphics::CellFieldResult u = mCellField.Update(slot.handle, mBuffer, count);
		if (!u.ok)
		{
			++mReadFailures;
			DIA_LOG_ERROR("CEL", "LayerUploader: CellField update failed: %s", u.error ? u.error : "unknown");
			ReleaseSlot(slot);
			return UploadResult::ReadFailed;
		}
		++mUploads;
		if (isBase) { StoreBaseValues(layer.id, count); }
		slot.lastVersion = version;
		didUpload = true;
		return UploadResult::Ok;
	}

	uint32_t LayerUploader::RegisterColourMap(const CellLayerDesc& base, float& rampMin, float& rampMax)
	{
		using namespace Dia::ValueSchema;
		const ValueInfo* info = mSchema ? mSchema->Find(base.schemaKey) : nullptr;

		if (base.kind == CellValueKind::Category)
		{
			uint32_t colours[CellFieldRegistry::kMaxPaletteColours];
			uint32_t count = 0;
			uint32_t fallback = 0x808080FF;
			StringCRC id;
			if (info != nullptr && mResolver != nullptr && info->mType == ValueType::Enum)
			{
				const ResolvedPalette p = mResolver->ResolvePalette(*info);
				for (unsigned i = 0; i < p.mEntries.Size() && count < CellFieldRegistry::kMaxPaletteColours; ++i) { colours[count++] = p.mEntries[i].mRgba8; }
				fallback = p.mFallbackRgba8;
				id = StringCRC(info->mFullKey.AsCStr());
			}
			if (count == 0)
			{
				colours[count++] = fallback;
				id = StringCRC(kDefaultPaletteName);
			}
			if (MarkColourMap(id.Value())) { mCellField.RegisterPalette(id, colours, count, fallback); }
			return id.Value();
		}

		Dia::Graphics::CellFieldRampStop stops[CellFieldRegistry::kMaxRampStops];
		uint32_t count = 0;
		const char* name = kFallbackRampName;
		rampMin = mObservedMin;
		rampMax = mObservedMax;
		if (info != nullptr && mResolver != nullptr)
		{
			const ResolvedRamp r = mResolver->ResolveRamp(*info);
			if (r.mRamp.mName.Length() > 0) { name = r.mRamp.mName.AsCStr(); }
			for (unsigned i = 0; i < r.mRamp.mStopCount && count < CellFieldRegistry::kMaxRampStops; ++i)
			{
				stops[count].position = r.mRamp.mStops[i].mT;
				stops[count].rgba8 = r.mRamp.mStops[i].mRgba8;
				++count;
			}
			if (info->mHasMin && info->mHasMax) { rampMin = r.mMin; rampMax = r.mMax; }
		}
		if (count == 0)
		{
			stops[0].position = 0.0f; stops[0].rgba8 = 0x000000FF;
			stops[1].position = 1.0f; stops[1].rgba8 = 0xFFFFFFFF;
			count = 2;
		}
		const StringCRC id(name);
		if (MarkColourMap(id.Value())) { mCellField.RegisterRamp(id, stops, count); }
		return id.Value();
	}

	UploadResult LayerUploader::Update(const CellLayerDesc* base, const CellLayerRegistry& layers)
	{
		using namespace Dia::ValueSchema;
		if (base == nullptr)
		{
			for (Slot& s : mSlots) { ReleaseSlot(s); }
			mRequest = Dia::Core::CellFieldDrawRequest();
			mBaseCopyCount = 0;
			return UploadResult::NoBase;
		}
		if (!LayerReadable(*base))
		{
			++mReadFailures;
			mBaseCopyCount = 0;
			DIA_LOG_ERROR("CEL", "LayerUploader: base layer 0x%x has no valid grid or read/version delegate", base->id.Value());
			return UploadResult::ReadFailed;
		}

		const ValueInfo* info = mSchema ? mSchema->Find(base->schemaKey) : nullptr;
		const CellLayerDesc* elevation = nullptr;
		if (const char* elevId = UiString(info, "elevation_layer"))
		{
			const CellLayerDesc* e = layers.Find(StringCRC(elevId));
			if (e != nullptr && e->id != base->id && e->grid == base->grid) { elevation = e; }
		}

		// Keep only the base and its elevation layer; destroy whatever else is held.
		for (Slot& s : mSlots)
		{
			if (s.held && s.id != base->id && (elevation == nullptr || s.id != elevation->id)) { ReleaseSlot(s); }
		}
		auto acquire = [this](const CellLayerDesc& l) -> Slot*
		{
			Slot* s = FindSlot(l.id);
			return s ? s : FreeSlot();
		};

		bool didUpload = false;
		Slot* baseSlot = acquire(*base);
		const UploadResult baseResult = Sync(*baseSlot, *base, true, didUpload);
		if (baseResult != UploadResult::Ok) { mBaseCopyCount = 0; return baseResult; }

		UploadResult result = UploadResult::Ok;
		bool elevationReady = false;
		if (elevation != nullptr)
		{
			Slot* elevSlot = acquire(*elevation);
			result = Sync(*elevSlot, *elevation, false, didUpload);
			elevationReady = (result == UploadResult::Ok);
		}

		const bool elevationChanged = elevationReady != mRequest.elevationLayer.IsValid();
		if (!didUpload && !elevationChanged && mRequest.layer.index == baseSlot->handle.index && mRequest.layer.generation == baseSlot->handle.generation)
		{
			++mSkippedFrames;
			return result;
		}

		Dia::Core::CellFieldDrawRequest req;
		req.layer = baseSlot->handle;
		float rampMin = mObservedMin, rampMax = mObservedMax;
		req.colourMap = RegisterColourMap(*base, rampMin, rampMax);
		req.rampMin = rampMin;
		req.rampMax = rampMax;
		if (info != nullptr && info->mUi.isObject())
		{
			const Json::Value& inset = info->mUi["inset"];
			if (inset.isNumeric())
			{
				float v = inset.asFloat();
				req.inset = v < 0.0f ? 0.0f : (v > 0.499f ? 0.499f : v);
			}
			uint32_t colour = 0;
			if (const char* backdrop = UiString(info, "backdrop"))
			{
				if (ParseHexColour(backdrop, colour)) { req.backdropColour = colour; }
			}
		}
		if (elevationReady)
		{
			req.elevationLayer = FindSlot(elevation->id)->handle;
			req.hillshadeStrength = 1.0f;
		}
		mRequest = req;
		return result;
	}
}
