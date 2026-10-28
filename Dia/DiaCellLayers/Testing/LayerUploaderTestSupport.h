#pragma once

// Test helpers for LayerUploader (Testing/ convention): recording CellField command sink, stub layer provider with
// call counters, an owning hex-sized test grid, and an in-memory schema + ramp fixture.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <string>
#include <vector>

namespace Dia::CellLayers::Testing
{
	// Owns the arrays a CellGridDesc borrows. Valid for CellFieldRegistry::CreateLayer.
	struct TestGrid
	{
		std::vector<float>		centres;
		std::vector<int32_t>	neighbours;
		Dia::Graphics::CellGridDesc desc;

		TestGrid(uint16_t w, uint16_t h)
			: centres(static_cast<size_t>(w) * h * 2, 0.0f)
			, neighbours(static_cast<size_t>(w) * h * 6, -1)
		{
			desc.topology = Dia::Core::StringCRC("hex.flat.oddq");
			desc.width = w;
			desc.height = h;
			desc.cellSize = 1.0f;
			desc.centresXY = centres.data();
			desc.neighbours = neighbours.data();
		}
		uint32_t CellCount() const { return static_cast<uint32_t>(desc.width) * desc.height; }
	};

	// Records what the render thread would receive on Drain.
	class RecordingSink : public Dia::Graphics::ICellFieldCommandSink
	{
	public:
		struct Update { Dia::Core::CellFieldLayerHandle handle; std::vector<float> values; };
		uint32_t				creates = 0;
		uint32_t				destroys = 0;
		std::vector<Update>		updates;

		void OnCreate(Dia::Core::CellFieldLayerHandle, const Dia::Graphics::CellGridDesc&, Dia::Graphics::CellFieldValueKind) override { ++creates; }
		void OnUpdate(Dia::Core::CellFieldLayerHandle h, const float* v, uint32_t n) override { updates.push_back({ h, std::vector<float>(v, v + n) }); }
		void OnDestroy(Dia::Core::CellFieldLayerHandle) override { ++destroys; }
	};

	// A layer provider: settable values and version, counts read calls.
	struct StubProvider
	{
		std::vector<float>	values;
		uint64_t			versionValue = 1;
		uint32_t			readCalls = 0;

		void Read(float* out, uint32_t count)
		{
			++readCalls;
			for (uint32_t i = 0; i < count; ++i) { out[i] = i < values.size() ? values[i] : 0.0f; }
		}
		uint64_t Version() { return versionValue; }

		CellLayerDesc MakeDesc(const char* id, const Dia::Graphics::CellGridDesc* grid, CellValueKind kind = CellValueKind::Float, const char* schemaKey = nullptr)
		{
			CellLayerDesc d;
			d.id = Dia::Core::StringCRC(id);
			d.schemaKey = Dia::Core::StringCRC(schemaKey ? schemaKey : id);
			d.kind = kind;
			d.grid = grid;
			d.read = fastdelegate::MakeDelegate(this, &StubProvider::Read);
			d.version = fastdelegate::MakeDelegate(this, &StubProvider::Version);
			return d;
		}
	};

	// In-memory schema + a tiny ramp library ("sequential.grey", "test.heat"). Add entries, then Publish().
	// Entries are for-format "cells", so the schema full key of key K is "cells/K".
	struct UploaderSchemaFixture
	{
		Dia::ValueSchema::ValueSchemaRegistry	schema;
		Dia::ValueSchema::RampRegistry			ramps;
		Dia::ValueSchema::ValueSchemaService*	service = nullptr;

		static Dia::ValueSchema::RampData MakeRamp(const char* name, uint32_t a, uint32_t b)
		{
			Dia::ValueSchema::RampData r;
			r.mId = Dia::Core::StringCRC(name);
			Dia::ValueSchema::SetText(r.mName, name);
			r.mStopCount = 2;
			r.mStops[0].mT = 0.0f; r.mStops[0].mRgba8 = a;
			r.mStops[1].mT = 1.0f; r.mStops[1].mRgba8 = b;
			return r;
		}

		UploaderSchemaFixture()
		{
			ramps.Register(MakeRamp("sequential.grey", 0x000000FF, 0xFFFFFFFF), true, "test");
			ramps.Register(MakeRamp("test.heat", 0x0000FFFF, 0xFF0000FF), true, "test");
		}
		~UploaderSchemaFixture() { delete service; }

		void Add(const Dia::ValueSchema::ValueInfo& info) { mPending.push_back(info); }

		static Dia::ValueSchema::ValueInfo FloatEntry(const char* key, double mn, double mx, const char* ramp = nullptr)
		{
			Dia::ValueSchema::ValueInfo i = Dia::ValueSchema::Testing::MakeValueInfo("cells", key);
			i.mHasMin = true; i.mMin = mn; i.mHasMax = true; i.mMax = mx;
			if (ramp) { i.mUi["ramp"] = ramp; }
			return i;
		}
		static Dia::ValueSchema::ValueInfo EnumEntry(const char* key, unsigned count)
		{
			Dia::ValueSchema::ValueInfo i = Dia::ValueSchema::Testing::MakeValueInfo("cells", key);
			i.mType = Dia::ValueSchema::ValueType::Enum;
			i.mEnumValues.Reserve(count);
			for (unsigned n = 0; n < count; ++n)
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, (std::string("v") + std::to_string(n)).c_str());
				i.mEnumValues.Add(v);
			}
			return i;
		}
		const Dia::ValueSchema::ValueSchemaService& Publish()
		{
			for (const auto& p : mPending) { schema.Register(p); }
			schema.Publish();
			ramps.Publish();
			service = new Dia::ValueSchema::ValueSchemaService(schema, &ramps);
			return *service;
		}
	private:
		std::vector<Dia::ValueSchema::ValueInfo> mPending;
	};
}
