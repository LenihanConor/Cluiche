#include <DiaCellLayers/LayerReader.h>

namespace Dia::CellLayers
{
	using Dia::Core::StringCRC;

	LayerReader::LayerReader(const CellLayerRegistry& registry, uint32_t maxSlots)
		: mRegistry(registry)
	{
		if (maxSlots < 1) { maxSlots = 1; }
		if (maxSlots > CellLayerRegistry::kMaxLayers) { maxSlots = CellLayerRegistry::kMaxLayers; }
		mSlotCount = maxSlots;
		mSlots = new Slot[mSlotCount];
	}

	LayerReader::~LayerReader()
	{
		for (uint32_t i = 0; i < mSlotCount; ++i) { delete[] mSlots[i].buffer; }
		delete[] mSlots;
	}

	void LayerReader::Invalidate()
	{
		for (uint32_t i = 0; i < mSlotCount; ++i)
		{
			mSlots[i].used = false;	// keep the buffer for reuse
		}
	}

	LayerReader::Slot* LayerReader::Acquire(StringCRC id)
	{
		Slot* free = nullptr;
		Slot* oldest = nullptr;
		for (uint32_t i = 0; i < mSlotCount; ++i)
		{
			Slot& s = mSlots[i];
			if (s.used && s.id == id) { return &s; }
			if (!s.used) { if (free == nullptr) { free = &s; } }
			else if (oldest == nullptr || s.lastUse < oldest->lastUse) { oldest = &s; }
		}
		return free != nullptr ? free : oldest;	// least recently used when full
	}

	LayerValues LayerReader::Read(StringCRC layerId)
	{
		LayerValues out;
		const CellLayerDesc* layer = mRegistry.Find(layerId);
		if (layer == nullptr || layer->grid == nullptr || layer->grid->width == 0 || layer->grid->height == 0
			|| layer->read.empty() || layer->version.empty())
		{
			return out;
		}

		IViewValueSource* source = mRegistry.ViewValueSource();
		const bool fromSource = source != nullptr && !source->IsLive();
		const uint64_t epoch = source != nullptr ? source->ViewEpoch() : 0;
		const uint64_t version = layer->version();
		const uint32_t count = static_cast<uint32_t>(layer->grid->width) * layer->grid->height;

		Slot* slot = Acquire(layerId);
		slot->lastUse = ++mTick;
		out.count = count;
		out.version = version;

		if (slot->used && slot->id == layerId && slot->grid == layer->grid && slot->count == count
			&& slot->version == version && slot->epoch == epoch && slot->fromSource == fromSource)
		{
			++mCacheHits;
			out.hasData = slot->hasData;
			out.values = slot->hasData ? slot->buffer : nullptr;
			return out;
		}

		if (slot->capacity < count)
		{
			delete[] slot->buffer;
			slot->buffer = new float[count];
			slot->capacity = count;
		}
		bool hasData = true;
		if (fromSource) { hasData = source->GetValues(*layer, slot->buffer, count); }
		else { layer->read(slot->buffer, count); }
		++mReads;

		slot->used = true;
		slot->id = layerId;
		slot->grid = layer->grid;
		slot->count = count;
		slot->version = version;
		slot->epoch = epoch;
		slot->fromSource = fromSource;
		slot->hasData = hasData;
		out.hasData = hasData;
		out.values = hasData ? slot->buffer : nullptr;
		return out;
	}
}
