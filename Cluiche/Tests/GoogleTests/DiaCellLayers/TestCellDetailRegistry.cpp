#include <gtest/gtest.h>
#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/Testing/CellDetailTestSupport.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaValueSchema/ValueInfo.h>

#include <stdio.h>
#include <string.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct SelectionDetailRegistryTest : ::testing::Test
	{
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		Dia::Graphics::CellGridDesc gridA = MakeGrid(4, 3);
		Dia::Graphics::CellGridDesc gridB = MakeGrid(2, 2);
		CellDetailRegistry reg{ &FakePuIdFn };
		CellDetailResult out;

		void SetUp() override
		{
			FakePuId() = 1;
			auto info = UploaderSchemaFixture::FloatEntry("temp", -50, 50);
			Dia::ValueSchema::SetText(info.mLabel, "Temperature");
			Dia::ValueSchema::SetText(info.mUnit, "C");
			Dia::ValueSchema::SetText(info.mHint, "Mean air temperature");
			fx.Add(info);
			service = &fx.Publish();
		}
	};
}

TEST_F(SelectionDetailRegistryTest, SingleProviderRowsUnderProviderSection)
{
	StubDetailProvider p; p.rowsToEmit = 2;
	EXPECT_EQ(reg.Register(&gridA, "Ecology", p.Delegate()), CellDetailRegisterResult::Ok);
	EXPECT_EQ(reg.Build(&gridA, 7, service, out), 1u);
	EXPECT_EQ(p.calls, 1u);
	EXPECT_EQ(p.lastCell, 7u);
	ASSERT_EQ(out.Count(), 3u);
	EXPECT_EQ(out.At(0).kind, CellDetailRowKind::Section);
	EXPECT_STREQ(out.At(0).label.AsCStr(), "Ecology");
	EXPECT_EQ(out.At(1).kind, CellDetailRowKind::Text);
	EXPECT_STREQ(out.At(1).label.AsCStr(), "row0");
	EXPECT_STREQ(out.At(1).text.AsCStr(), "v0");
}

TEST_F(SelectionDetailRegistryTest, RowValueTakesLabelUnitTooltipFromSchema)
{
	StubDetailProvider p; p.valueKey = StringCRC("cells/temp"); p.valueToEmit = 12.5f;
	reg.Register(&gridA, "Climate", p.Delegate());
	reg.Build(&gridA, 0, service, out);
	ASSERT_EQ(out.Count(), 2u);
	const CellDetailRow& r = out.At(1);
	EXPECT_EQ(r.kind, CellDetailRowKind::Value);
	EXPECT_TRUE(r.resolved);
	EXPECT_STREQ(r.label.AsCStr(), "Temperature");
	EXPECT_STREQ(r.unit.AsCStr(), "C");
	EXPECT_STREQ(r.tooltip.AsCStr(), "Mean air temperature");
	EXPECT_FLOAT_EQ(r.value, 12.5f);
	EXPECT_TRUE(r.schemaKey == StringCRC("cells/temp"));
}

TEST_F(SelectionDetailRegistryTest, UnresolvedKeyKeepsRowWithEmptyLabelAndFlag)
{
	StubDetailProvider p; p.valueKey = StringCRC("cells/nope"); p.valueToEmit = 3.0f;
	reg.Register(&gridA, "X", p.Delegate());
	reg.Build(&gridA, 0, service, out);
	ASSERT_EQ(out.Count(), 2u);
	EXPECT_FALSE(out.At(1).resolved);
	EXPECT_STREQ(out.At(1).label.AsCStr(), "");
	EXPECT_FLOAT_EQ(out.At(1).value, 3.0f);
	// null schema behaves the same
	reg.Build(&gridA, 0, nullptr, out);
	ASSERT_EQ(out.Count(), 2u);
	EXPECT_FALSE(out.At(1).resolved);
}

TEST_F(SelectionDetailRegistryTest, ProviderSectionTitlesAreRows)
{
	StubDetailProvider p; p.sectionTitle = "Change ledger"; p.rowsToEmit = 1;
	reg.Register(&gridA, "Ecology", p.Delegate());
	reg.Build(&gridA, 0, service, out);
	ASSERT_EQ(out.Count(), 3u);
	EXPECT_EQ(out.At(1).kind, CellDetailRowKind::Section);
	EXPECT_STREQ(out.At(1).label.AsCStr(), "Change ledger");
}

TEST_F(SelectionDetailRegistryTest, SectionsAppearInRegistrationOrder)
{
	StubDetailProvider a, b, c; a.rowsToEmit = b.rowsToEmit = c.rowsToEmit = 1;
	reg.Register(&gridA, "Second", b.Delegate());
	reg.Register(&gridA, "First", a.Delegate());
	reg.Register(&gridA, "Third", c.Delegate());
	EXPECT_EQ(reg.Build(&gridA, 0, service, out), 3u);
	ASSERT_EQ(out.Count(), 6u);
	EXPECT_STREQ(out.At(0).label.AsCStr(), "Second");
	EXPECT_STREQ(out.At(2).label.AsCStr(), "First");
	EXPECT_STREQ(out.At(4).label.AsCStr(), "Third");
}

TEST_F(SelectionDetailRegistryTest, ProvidersArePerGrid)
{
	StubDetailProvider a, b; a.rowsToEmit = b.rowsToEmit = 1;
	reg.Register(&gridA, "A", a.Delegate());
	reg.Register(&gridB, "B", b.Delegate());
	EXPECT_EQ(reg.ProviderCountForGrid(&gridA), 1u);
	reg.Build(&gridA, 0, service, out);
	EXPECT_EQ(a.calls, 1u);
	EXPECT_EQ(b.calls, 0u);
	EXPECT_STREQ(out.At(0).label.AsCStr(), "A");
}

TEST_F(SelectionDetailRegistryTest, DuplicateNameOnSameGridRejectedButOtherGridAllowed)
{
	StubDetailProvider a, b;
	EXPECT_EQ(reg.Register(&gridA, "Eco", a.Delegate()), CellDetailRegisterResult::Ok);
	EXPECT_EQ(reg.Register(&gridA, "Eco", b.Delegate()), CellDetailRegisterResult::Duplicate);
	EXPECT_EQ(reg.Register(&gridB, "Eco", b.Delegate()), CellDetailRegisterResult::Ok);
	EXPECT_EQ(reg.ProviderCount(), 2u);
	reg.Build(&gridA, 0, service, out);
	EXPECT_EQ(a.calls, 1u);
	EXPECT_EQ(b.calls, 0u);
}

TEST_F(SelectionDetailRegistryTest, CapacityReached)
{
	StubDetailProvider p;
	for (uint32_t i = 0; i < CellDetailRegistry::kMaxProviders; ++i)
	{
		char n[16]; snprintf(n, sizeof(n), "p%u", i);
		EXPECT_EQ(reg.Register(&gridA, n, p.Delegate()), CellDetailRegisterResult::Ok);
	}
	EXPECT_EQ(reg.Register(&gridA, "extra", p.Delegate()), CellDetailRegisterResult::CapacityReached);
	EXPECT_EQ(reg.ProviderCount(), CellDetailRegistry::kMaxProviders);
}

TEST_F(SelectionDetailRegistryTest, WrongProcessingUnitRejected)
{
	StubDetailProvider p;
	FakePuId() = 2;
	EXPECT_EQ(reg.Register(&gridA, "Eco", p.Delegate()), CellDetailRegisterResult::WrongProcessingUnit);
	EXPECT_EQ(reg.ProviderCount(), 0u);
	FakePuId() = 1;	// the registry is constructed before SetUp: leave the fake PU as other tests expect
}

TEST_F(SelectionDetailRegistryTest, UnregisterStopsCalls)
{
	StubDetailProvider a, b;
	reg.Register(&gridA, "A", a.Delegate());
	reg.Register(&gridA, "B", b.Delegate());
	EXPECT_TRUE(reg.Unregister(&gridA, "A"));
	EXPECT_FALSE(reg.Unregister(&gridA, "A"));
	reg.Build(&gridA, 0, service, out);
	EXPECT_EQ(a.calls, 0u);
	EXPECT_EQ(b.calls, 1u);
	EXPECT_EQ(reg.UnregisterAll(&gridA), 1u);
	EXPECT_EQ(reg.ProviderCount(), 0u);
	EXPECT_EQ(reg.Register(&gridA, "A", a.Delegate()), CellDetailRegisterResult::Ok);
}

TEST_F(SelectionDetailRegistryTest, RowCapDropsExtrasAndWarnsOncePerProvider)
{
	LogCapture log;
	StubDetailProvider p; p.rowsToEmit = 100;
	reg.Register(&gridA, "Big", p.Delegate());
	reg.Build(&gridA, 0, service, out);
	EXPECT_EQ(out.Count(), 1u + CellDetailBuilder::kMaxRowsPerProvider);	// provider section + 64 rows
	EXPECT_EQ(log.Warnings(), 1u);
	reg.Build(&gridA, 0, service, out);
	reg.Build(&gridA, 1, service, out);
	EXPECT_EQ(out.Count(), 1u + CellDetailBuilder::kMaxRowsPerProvider);
	EXPECT_EQ(log.Warnings(), 1u);
	// exactly at the cap: no warning
	StubDetailProvider ok; ok.rowsToEmit = CellDetailBuilder::kMaxRowsPerProvider;
	reg.Register(&gridB, "Exact", ok.Delegate());
	reg.Build(&gridB, 0, service, out);
	EXPECT_EQ(log.Warnings(), 1u);
	// a second overflowing provider warns once, independently
	StubDetailProvider big2; big2.rowsToEmit = 65;
	reg.Register(&gridB, "Big2", big2.Delegate());
	reg.Build(&gridB, 0, service, out);
	reg.Build(&gridB, 0, service, out);
	EXPECT_EQ(log.Warnings(), 2u);
}

TEST_F(SelectionDetailRegistryTest, BuilderCapReportsDropped)
{
	CellDetailResult r;
	CellDetailBuilder b(r, nullptr);
	for (int i = 0; i < 70; ++i) { b.Row("l", "t"); }
	EXPECT_EQ(b.Added(), 64u);
	EXPECT_EQ(b.Dropped(), 6u);
	EXPECT_TRUE(b.Truncated());
	EXPECT_EQ(r.Count(), 64u);
}

TEST_F(SelectionDetailRegistryTest, ShouldRunProvidersGating)
{
	CellSelection sel; sel.Reset(&gridA);
	uint32_t cell = 99;
	EXPECT_FALSE(ShouldRunProviders(nullptr, true, cell));
	EXPECT_FALSE(ShouldRunProviders(&sel, true, cell));		// 0 cells
	sel.Set(5, true);
	EXPECT_FALSE(ShouldRunProviders(&sel, false, cell));	// card closed
	EXPECT_TRUE(ShouldRunProviders(&sel, true, cell));
	EXPECT_EQ(cell, 5u);
	sel.Set(6, true);
	EXPECT_FALSE(ShouldRunProviders(&sel, true, cell));		// 2 cells
}

TEST_F(SelectionDetailRegistryTest, ProviderOnlyInvokedByBuild)
{
	StubDetailProvider p; p.rowsToEmit = 1;
	reg.Register(&gridA, "A", p.Delegate());
	reg.Unregister(&gridB, "none");
	reg.ProviderCountForGrid(&gridA);
	CellSelection sel; sel.Reset(&gridA); sel.Set(1, true);
	uint32_t cell = 0;
	ShouldRunProviders(&sel, true, cell);
	EXPECT_EQ(p.calls, 0u);
	reg.Build(&gridA, cell, service, out);
	EXPECT_EQ(p.calls, 1u);
}

TEST_F(SelectionDetailRegistryTest, LayerRegistryOwnsDetailRegistry)
{
	CellLayerRegistry layers(service, &FakePuIdFn);
	StubDetailProvider p; p.rowsToEmit = 1;
	EXPECT_EQ(layers.Details().Register(&gridA, "A", p.Delegate()), CellDetailRegisterResult::Ok);
	FakePuId() = 2;
	EXPECT_EQ(layers.Details().Register(&gridA, "B", p.Delegate()), CellDetailRegisterResult::WrongProcessingUnit);
	FakePuId() = 1;
	const CellLayerRegistry& cl = layers;
	EXPECT_EQ(cl.Details().ProviderCount(), 1u);
}
