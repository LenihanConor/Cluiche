#pragma once

// HotkeyTable: key -> base layer id for one grid. Hotkeys come from each layer's schema ui.hotkey (a one-character
// string, unique per grid). A clash logs a Warning (channel CEL) and the later layer gets no hotkey. The table only
// maps key -> layer id; the caller runs `cells.layer.base <id>`. Rebuild when the active grid or the registry changes.
// Also provides the picker numbering (base layers 1..9 within a group, in registration order, per grid).
// Generic: no game types (CL-005), no singletons (CL-006). Borrows the registry (and schema); both must outlive it.

#include <DiaCellLayers/CellLayerRegistry.h>

#include <stdint.h>

namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellLayers
{
	class HotkeyTable
	{
	public:
		static const uint32_t kMaxGroupNumber = 9;

		// schema may be null (no hotkeys). Neither is owned.
		HotkeyTable(const CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService* schema);

		// Rebuilds from the grid's layers; returns the number of bound keys.
		uint32_t			Rebuild(const Dia::Graphics::CellGridDesc* grid);
		uint32_t			BindingCount() const { return mCount; }
		uint32_t			ClashCount() const { return mClashes; }

		// Null if the key is unbound.
		const CellLayerDesc*	Lookup(char key) const;
		// 0 if the layer has no hotkey in this table.
		char				HotkeyOf(Dia::Core::StringCRC layerId) const;

		// 1..kMaxGroupNumber: position of the layer among the grid's layers sharing its group; 0 if beyond the 9th,
		// not found, or on another grid.
		static uint32_t		GroupNumber(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, Dia::Core::StringCRC layerId);
		// The viewer shows a grid selector only when registry.IsMultiGrid().

	private:
		struct Binding
		{
			char				key = 0;
			const CellLayerDesc*	layer = nullptr;
		};

		const CellLayerRegistry*	mRegistry;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		uint32_t			mCount = 0;
		uint32_t			mClashes = 0;
		Binding				mBindings[CellLayerRegistry::kMaxLayers];
	};
}
