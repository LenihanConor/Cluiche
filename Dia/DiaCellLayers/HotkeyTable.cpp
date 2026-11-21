#include <DiaCellLayers/HotkeyTable.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <string.h>

namespace Dia::CellLayers
{
	using Dia::Core::StringCRC;

	HotkeyTable::HotkeyTable(const CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService* schema)
		: mRegistry(&registry)
		, mSchema(schema)
	{
	}

	uint32_t HotkeyTable::Rebuild(const Dia::Graphics::CellGridDesc* grid)
	{
		mCount = 0;
		mClashes = 0;
		if (mSchema == nullptr) { return 0; }

		const uint32_t layerCount = mRegistry->LayerCountForGrid(grid);
		for (uint32_t i = 0; i < layerCount; ++i)
		{
			const CellLayerDesc* layer = mRegistry->LayerOfGrid(grid, i);
			const Dia::ValueSchema::ValueInfo* info = mSchema->Find(layer->schemaKey);
			if (info == nullptr) { continue; }
			const Json::Value& hk = info->mUi["hotkey"];
			if (!hk.isString() || hk.asString().empty()) { continue; }
			const char key = hk.asString()[0];

			const CellLayerDesc* owner = Lookup(key);
			if (owner != nullptr)
			{
				++mClashes;
				DIA_LOG_WARNING("CEL", "Hotkey '%c' for layer '%s' clashes with layer '%s'; '%s' gets no hotkey",
					key, layer->id.AsChar(), owner->id.AsChar(), layer->id.AsChar());
				continue;
			}
			mBindings[mCount].key = key;
			mBindings[mCount].layer = layer;
			++mCount;
		}
		return mCount;
	}

	const CellLayerDesc* HotkeyTable::Lookup(char key) const
	{
		for (uint32_t i = 0; i < mCount; ++i)
		{
			if (mBindings[i].key == key) { return mBindings[i].layer; }
		}
		return nullptr;
	}

	char HotkeyTable::HotkeyOf(StringCRC layerId) const
	{
		for (uint32_t i = 0; i < mCount; ++i)
		{
			if (mBindings[i].layer->id == layerId) { return mBindings[i].key; }
		}
		return 0;
	}

	uint32_t HotkeyTable::GroupNumber(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, StringCRC layerId)
	{
		const CellLayerDesc* target = registry.Find(layerId);
		if (target == nullptr || target->grid != grid) { return 0; }

		uint32_t number = 0;
		const uint32_t count = registry.LayerCountForGrid(grid);
		for (uint32_t i = 0; i < count; ++i)
		{
			const CellLayerDesc* l = registry.LayerOfGrid(grid, i);
			if (strcmp(l->group.AsCStr(), target->group.AsCStr()) != 0) { continue; }
			++number;
			if (l == target) { return number <= kMaxGroupNumber ? number : 0; }
		}
		return 0;
	}
}
