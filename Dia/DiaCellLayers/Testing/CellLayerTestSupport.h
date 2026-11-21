#pragma once

// Test helpers for DiaCellLayers (Testing/ convention): stub layer/overlay builders and a fake PU id source.

#include <DiaCellLayers/CellLayerRegistry.h>

namespace Dia::CellLayers::Testing
{
	inline void StubRead(float*, uint32_t) {}
	inline uint64_t StubVersion() { return 0; }
	inline void StubDraw(Dia::Core::IDebugDraw&, const void*) {}

	// Builds a layer desc with no-op callbacks. schemaKey defaults to "test/<id>".
	inline CellLayerDesc MakeLayer(const char* id, const Dia::Graphics::CellGridDesc* grid, const char* schemaKey = nullptr,
		CellValueKind kind = CellValueKind::Float, const char* group = "Test")
	{
		CellLayerDesc d;
		d.id = Dia::Core::StringCRC(id);
		d.schemaKey = Dia::Core::StringCRC(schemaKey ? schemaKey : id);
		d.group.Append(group);
		d.kind = kind;
		d.grid = grid;
		d.read = fastdelegate::FastDelegate2<float*, uint32_t>(&StubRead);
		d.version = fastdelegate::FastDelegate0<uint64_t>(&StubVersion);
		return d;
	}

	inline CellOverlayDesc MakeOverlay(const char* id, const char* label = "Overlay", const char* group = "Test")
	{
		CellOverlayDesc o;
		o.id = Dia::Core::StringCRC(id);
		o.group.Append(group);
		o.label.Append(label);
		o.draw = fastdelegate::FastDelegate2<Dia::Core::IDebugDraw&, const void*>(&StubDraw);
		return o;
	}

	// Fake "current PU id" source for the registry's injectable owner check.
	inline uint64_t& FakePuId() { static uint64_t sId = 1; return sId; }
	inline uint64_t FakePuIdFn() { return FakePuId(); }
}
