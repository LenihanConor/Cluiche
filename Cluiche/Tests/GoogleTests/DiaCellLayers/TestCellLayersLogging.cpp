#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerLegend.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/HotkeyTable.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#include <stdio.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct CellLayersLoggingTest : ::testing::Test
	{
		LogCapture log;
		TestGrid grid{ 4, 3 };
		TestGrid gridB{ 3, 3 };
		StubProvider p;
		void SetUp() override { FakePuId() = 1; }
	};
}

TEST_F(CellLayersLoggingTest, HotkeyClashWarnsOncePerClash)
{
	UploaderSchemaFixture fx;
	for (const char* k : { "a", "b", "c" })
	{
		auto e = UploaderSchemaFixture::FloatEntry(k, 0, 1);
		e.mUi["hotkey"] = "t";
		fx.Add(e);
	}
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid.desc, "cells/a"));
	reg.Register(MakeLayer("b", &grid.desc, "cells/b"));
	reg.Register(MakeLayer("c", &grid.desc, "cells/c"));
	HotkeyTable table(reg, &svc);
	table.Rebuild(&grid.desc);
	EXPECT_EQ(table.ClashCount(), 2u);
	EXPECT_EQ(log.Warnings("clashes"), 2u);
	EXPECT_EQ(log.Errors(), 0u);
}

TEST_F(CellLayersLoggingTest, NoClashNoWarning)
{
	UploaderSchemaFixture fx;
	auto a = UploaderSchemaFixture::FloatEntry("a", 0, 1); a.mUi["hotkey"] = "t"; fx.Add(a);
	auto b = UploaderSchemaFixture::FloatEntry("b", 0, 1); b.mUi["hotkey"] = "m"; fx.Add(b);
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid.desc, "cells/a"));
	reg.Register(MakeLayer("b", &grid.desc, "cells/b"));
	HotkeyTable table(reg, &svc);
	table.Rebuild(&grid.desc);
	EXPECT_EQ(log.Warnings(), 0u);
}

TEST_F(CellLayersLoggingTest, RebuildRewarnsOnEachRebuild)
{
	UploaderSchemaFixture fx;
	for (const char* k : { "a", "b" }) { auto e = UploaderSchemaFixture::FloatEntry(k, 0, 1); e.mUi["hotkey"] = "x"; fx.Add(e); }
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid.desc, "cells/a"));
	reg.Register(MakeLayer("b", &grid.desc, "cells/b"));
	HotkeyTable table(reg, &svc);
	table.Rebuild(&grid.desc);
	table.Rebuild(&grid.desc);
	EXPECT_EQ(log.Warnings("clashes"), 2u);
	EXPECT_EQ(table.ClashCount(), 1u);
}

TEST_F(CellLayersLoggingTest, DuplicateLayerLogsError)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid.desc));
	reg.Register(MakeLayer("a", &grid.desc));
	EXPECT_EQ(log.Errors("duplicate"), 1u);
}

TEST_F(CellLayersLoggingTest, DuplicateOverlayLogsError)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.RegisterOverlay(MakeOverlay("o"));
	reg.RegisterOverlay(MakeOverlay("o"));
	EXPECT_EQ(log.Errors("duplicate"), 1u);
}

TEST_F(CellLayersLoggingTest, LayerCapacityLogsErrorPerRejection)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	char id[16];
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxLayers; ++i) { snprintf(id, sizeof(id), "l%u", i); reg.Register(MakeLayer(id, &grid.desc)); }
	EXPECT_EQ(log.Errors(), 0u);
	reg.Register(MakeLayer("over1", &grid.desc));
	reg.Register(MakeLayer("over2", &grid.desc));
	EXPECT_EQ(log.Errors("capacity"), 2u);
}

TEST_F(CellLayersLoggingTest, OverlayCapacityLogsError)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	char id[16];
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxOverlays; ++i) { snprintf(id, sizeof(id), "o%u", i); reg.RegisterOverlay(MakeOverlay(id)); }
	reg.RegisterOverlay(MakeOverlay("over"));
	EXPECT_EQ(log.Errors("capacity"), 1u);
}

TEST_F(CellLayersLoggingTest, WrongProcessingUnitLogsErrorForLayerAndOverlay)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	FakePuId() = 7;
	reg.Register(MakeLayer("a", &grid.desc));
	reg.RegisterOverlay(MakeOverlay("o"));
	FakePuId() = 1;
	EXPECT_EQ(log.Errors("rejected: called from PU"), 2u);
	EXPECT_EQ(reg.LayerCount(), 0u);
}

TEST_F(CellLayersLoggingTest, UnregisterFromAnyPuDoesNotLog)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid.desc));
	FakePuId() = 7;
	reg.Unregister(StringCRC("a"));
	reg.Unregister(StringCRC("nope"));
	FakePuId() = 1;
	EXPECT_EQ(log.Errors(), 0u);
}

TEST_F(CellLayersLoggingTest, SecondViewSourceInstallLogsError)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	StubViewValueSource s1, s2;
	EXPECT_TRUE(reg.SetViewValueSource(&s1));
	EXPECT_EQ(log.Errors(), 0u);
	EXPECT_FALSE(reg.SetViewValueSource(&s2));
	EXPECT_EQ(log.Errors("already installed"), 1u);
	EXPECT_EQ(reg.ViewValueSource(), &s1);
}

TEST_F(CellLayersLoggingTest, NullViewSourceInstallLogsError)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	EXPECT_FALSE(reg.SetViewValueSource(nullptr));
	EXPECT_EQ(log.Errors("null source"), 1u);
}

TEST_F(CellLayersLoggingTest, InstallAfterClearSucceedsWithoutError)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	StubViewValueSource s1, s2;
	reg.SetViewValueSource(&s1);
	reg.ClearViewValueSource();
	EXPECT_TRUE(reg.SetViewValueSource(&s2));
	EXPECT_EQ(log.Errors(), 0u);
}

TEST_F(CellLayersLoggingTest, UnresolvedSchemaKeyWarnsOncePerKeyAcrossManyBuilds)
{
	UploaderSchemaFixture fx;
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 1));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	CellLayerLegend legend;
	for (int i = 0; i < 20; ++i)
	{
		legend.Build(p.MakeDesc("g1", &grid.desc), &svc, resolver, false, 0, 0);
		legend.Build(p.MakeDesc("g2", &grid.desc), &svc, resolver, false, 0, 0);
	}
	EXPECT_EQ(log.Warnings("not found"), 2u);
}

TEST_F(CellLayersLoggingTest, UnresolvedKeyWarningMemoryIsBounded)
{
	UploaderSchemaFixture fx;
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 1));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	CellLayerLegend legend;
	char key[16];
	for (uint32_t i = 0; i < CellLayerLegend::kMaxWarnedKeys + 10; ++i)
	{
		snprintf(key, sizeof(key), "ghost%u", i);
		legend.Build(p.MakeDesc("g", &grid.desc, CellValueKind::Float, key), &svc, resolver, false, 0, 0);
	}
	EXPECT_EQ(legend.UnresolvedWarningCount(), CellLayerLegend::kMaxWarnedKeys);
	EXPECT_EQ(log.Warnings("not found"), CellLayerLegend::kMaxWarnedKeys);
}

TEST_F(CellLayersLoggingTest, NullSchemaLegendDoesNotWarn)
{
	UploaderSchemaFixture fx;
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	CellLayerLegend legend;
	legend.Build(p.MakeDesc("g", &grid.desc), nullptr, resolver, false, 0, 0);
	EXPECT_EQ(log.Warnings(), 0u);
}

TEST_F(CellLayersLoggingTest, UploaderReadFailureLogsErrorPerFailedUpdate)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	CellLayerDesc d = p.MakeDesc("a", &grid.desc);
	d.read = fastdelegate::FastDelegate2<float*, uint32_t>();
	reg.Register(d);
	Dia::Graphics::CellFieldRegistry cf;
	LayerUploader up(cf, nullptr);
	up.Update(reg.Find(StringCRC("a")), reg);
	up.Update(reg.Find(StringCRC("a")), reg);
	EXPECT_EQ(log.Errors("no valid grid or read/version delegate"), 2u);
	EXPECT_EQ(up.ReadFailures(), 2u);
}

#ifdef DIA_DEBUG
TEST_F(CellLayersLoggingTest, ViewerGridMismatchWarnsOnce)
{
	Dia::Graphics::CellFieldRegistry cf;
	StubProvider q;
	q.values = std::vector<float>(12, 1.0f);
	p.values = std::vector<float>(12, 1.0f);
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	LayerUploader up(cf, nullptr);
	CellLayerViewerDomain domain(reg, up, nullptr);
	FakeDebugDraw draw;
	reg.Register(p.MakeDesc("a", &grid.desc));
	domain.Update(draw);
	reg.Unregister(StringCRC("a"));
	q.values = std::vector<float>(9, 1.0f);
	reg.Register(q.MakeDesc("a", &gridB.desc));
	domain.Update(draw);
	domain.Update(draw);
	EXPECT_EQ(log.Warnings("different grid"), 1u);
	EXPECT_EQ(domain.ActiveGrid(), &gridB.desc);
}
#endif
