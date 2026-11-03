#pragma once

// Shared fixture for the DiaCellHistory tracking/command tests: a StubGrid registered in a CellLayerRegistry plus a recorder.

#include <gtest/gtest.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/Testing/BudgetSizing.h>
#include <DiaCellHistory/Testing/FakeClock.h>
#include <DiaCellHistory/Testing/StubGrid.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <memory>
#include <string>
#include <vector>

namespace HistoryRig
{
	inline std::string FloatEntry(const char* key, double mn, double mx, const char* historyBlock = nullptr)
	{
		std::string s = std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"L\",\"description\":\"D\",\"type\":\"float\",\"min\":"
			+ std::to_string(mn) + ",\"max\":" + std::to_string(mx);
		if (historyBlock) { s += std::string(",\"history\":") + historyBlock; }
		return s + "}";
	}
	// A float layer with no min/max.
	inline std::string RangelessFloatEntry(const char* key)
	{
		return std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"L\",\"description\":\"D\",\"type\":\"float\"}";
	}
	inline std::string EnumEntry(const char* key, int count, const char* historyBlock = nullptr)
	{
		std::string s = std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"L\",\"description\":\"D\",\"type\":\"enum\",\"values\":[";
		for (int i = 0; i < count; ++i)
		{
			if (i) { s += ","; }
			s += std::string("{\"id\":\"") + char('a' + i) + "\",\"label\":\"V\"}";
		}
		s += "]";
		if (historyBlock) { s += std::string(",\"history\":") + historyBlock; }
		return s + "}";
	}

	struct SchemaSet
	{
		Dia::ValueSchema::ValueSchemaRegistry registry;
		std::unique_ptr<Dia::ValueSchema::ValueSchemaService> service;

		bool Load(const std::vector<std::string>& entries)
		{
			std::string doc = "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"cells\",\"entries\":[";
			for (size_t i = 0; i < entries.size(); ++i) { if (i) { doc += ","; } doc += entries[i]; }
			doc += "]}";
			Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(doc.c_str(), "rig.json");
			if (!r.mOk) { ADD_FAILURE() << r.mFailure.mReason.AsCStr(); return false; }
			for (uint32_t i = 0; i < r.mEntries.Size(); ++i) { registry.Register(r.mEntries[i]); }
			registry.Publish();
			service = std::make_unique<Dia::ValueSchema::ValueSchemaService>(registry);
			return true;
		}
	};

	inline void RegisterWithKeys(Dia::CellHistory::Testing::StubGrid& g, Dia::CellLayers::CellLayerRegistry& reg, uint32_t from = 0)
	{
		for (uint32_t i = from; i < g.LayerCount(); ++i)
		{
			Dia::CellLayers::CellLayerDesc d;
			g.Describe(i, d);
			d.schemaKey = Dia::Core::StringCRC((std::string("cells/") + d.id.AsChar()).c_str());
			ASSERT_EQ(reg.Register(d), Dia::CellLayers::CellRegisterResult::Ok);
		}
	}

	struct Rig
	{
		Dia::CellHistory::Testing::StubGrid grid{ 4, 3 };	// 12 cells
		Dia::CellHistory::Testing::StubGrid other{ 2, 2 };	// a second grid (different size, same registry)
		SchemaSet schema;
		Dia::CellLayers::CellLayerRegistry registry;
		Dia::CellHistory::HistoryConfig cfg;
		std::unique_ptr<Dia::CellHistory::CellHistoryRecorder> rec;
		Dia::CellHistory::Testing::FakeClock clock;

		void Start(const Dia::CellHistory::RecorderBudgets* budgets = nullptr, bool registerOther = true)
		{
			RegisterWithKeys(grid, registry);
			if (registerOther && other.LayerCount() > 0)
			{
				RegisterWithKeys(other, registry);
			}
			rec = std::make_unique<Dia::CellHistory::CellHistoryRecorder>(registry, schema.service.get(), cfg, 99, budgets);
		}
		Dia::CellHistory::SampleResult Take()
		{
			Dia::CellHistory::Testing::FakeClockSample s = clock.Next();
			Dia::CellHistory::SampleInfo i; i.tick = s.tick; i.label = s.label; i.band = s.band;
			return rec->Sample(i);
		}
	};
}
