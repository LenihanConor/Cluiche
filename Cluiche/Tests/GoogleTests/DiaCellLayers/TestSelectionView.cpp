#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/ICellSelectionView.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

#include <memory>
#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
namespace Tools = Dia::CellLayers::SelectionTools;

namespace
{
	struct Collected { std::vector<uint32_t> cells; };
	void Collect(uint32_t cell, void* user) { static_cast<Collected*>(user)->cells.push_back(cell); }

	struct SelectionViewTest : ::testing::Test
	{
		TestGrid gridA{ 4, 3 };
		TestGrid gridB{ 5, 2 };
		StubProvider temp, soil;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = std::vector<float>(12, 1.0f);
			soil.values = std::vector<float>(10, 1.0f);
		}
		void AddBoth()
		{
			ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &gridA.desc)), CellRegisterResult::Ok);
			ASSERT_EQ(registry.Register(soil.MakeDesc("soil", &gridB.desc)), CellRegisterResult::Ok);
		}
	};
}

TEST_F(SelectionViewTest, ViewAgreesWithToolsOnTheSelection)
{
	AddBoth();
	CellSelection* sel = registry.SelectionFor(&gridA.desc);
	ASSERT_NE(sel, nullptr);
	Tools::SelectBox(*sel, 0, 0, 1, 1);
	const ICellSelectionView* view = registry.SelectionView(&gridA.desc);
	ASSERT_NE(view, nullptr);
	EXPECT_EQ(view->Count(), sel->Count());
	EXPECT_EQ(view->Count(), 4u);
	EXPECT_EQ(view->Version(), sel->Version());
	Collected viaView, viaSel;
	view->ForEach(&Collect, &viaView);
	sel->ForEach(&Collect, &viaSel);
	EXPECT_EQ(viaView.cells, viaSel.cells);
	ASSERT_EQ(viaView.cells.size(), 4u);
	for (uint32_t c : viaView.cells) { EXPECT_TRUE(view->Contains(c)); }
	EXPECT_FALSE(view->Contains(11));
}

TEST_F(SelectionViewTest, SameObjectOnEveryAccess)
{
	AddBoth();
	CellSelection* a = registry.SelectionFor(&gridA.desc);
	EXPECT_EQ(registry.SelectionFor(&gridA.desc), a);
	EXPECT_EQ(registry.SelectionView(&gridA.desc), static_cast<const ICellSelectionView*>(a));
}

TEST_F(SelectionViewTest, VersionChangesOnlyOnRealChange)
{
	AddBoth();
	CellSelection* sel = registry.SelectionFor(&gridA.desc);
	const ICellSelectionView* view = registry.SelectionView(&gridA.desc);
	const uint32_t v0 = view->Version();
	sel->Set(2, true);
	const uint32_t v1 = view->Version();
	EXPECT_NE(v1, v0);
	sel->Set(2, true);							// no change
	registry.SelectionFor(&gridA.desc);			// re-access: no change
	registry.SelectionView(&gridA.desc);
	EXPECT_EQ(view->Version(), v1);
	sel->Clear();
	EXPECT_NE(view->Version(), v1);
}

TEST_F(SelectionViewTest, TwoGridsAreIndependent)
{
	AddBoth();
	CellSelection* a = registry.SelectionFor(&gridA.desc);
	CellSelection* b = registry.SelectionFor(&gridB.desc);
	ASSERT_NE(a, b);
	a->Set(1, true);
	EXPECT_EQ(registry.SelectionView(&gridA.desc)->Count(), 1u);
	EXPECT_EQ(registry.SelectionView(&gridB.desc)->Count(), 0u);
	b->Set(3, true);
	b->Set(4, true);
	EXPECT_EQ(registry.SelectionView(&gridA.desc)->Count(), 1u);
	EXPECT_EQ(registry.SelectionView(&gridB.desc)->Count(), 2u);
}

TEST_F(SelectionViewTest, ChangedWidthClearsTheSelection)
{
	AddBoth();
	CellSelection* a = registry.SelectionFor(&gridA.desc);
	a->Set(1, true);
	ASSERT_EQ(registry.SelectionView(&gridA.desc)->Count(), 1u);
	const uint32_t v = registry.SelectionView(&gridA.desc)->Version();
	gridA.desc.width = 6;
	EXPECT_EQ(registry.SelectionView(&gridA.desc)->Count(), 0u);
	EXPECT_NE(registry.SelectionView(&gridA.desc)->Version(), v);
	EXPECT_EQ(registry.SelectionFor(&gridA.desc)->Width(), 6u);
}

TEST_F(SelectionViewTest, ChangedTopologyIdClearsTheSelection)
{
	AddBoth();
	registry.SelectionFor(&gridA.desc)->Set(1, true);
	gridA.desc.topology = StringCRC("square.4");
	EXPECT_EQ(registry.SelectionView(&gridA.desc)->Count(), 0u);
}

TEST_F(SelectionViewTest, UnknownOrLayerlessGridGivesNullView)
{
	TestGrid other(3, 3);
	AddBoth();
	EXPECT_EQ(registry.SelectionView(nullptr), nullptr);
	EXPECT_EQ(registry.SelectionView(&other.desc), nullptr);
	// Known grid with layers but no selection entry yet: no view until SelectionFor creates it.
	EXPECT_EQ(registry.SelectionView(&gridA.desc), nullptr);
	// A selection exists for a grid with no layers: still no view (no stale data).
	CellSelection* o = registry.SelectionFor(&other.desc);
	ASSERT_NE(o, nullptr);
	o->Set(1, true);
	EXPECT_EQ(registry.SelectionView(&other.desc), nullptr);
	// Layers removed from a grid: view goes null.
	registry.SelectionFor(&gridA.desc)->Set(1, true);
	ASSERT_NE(registry.SelectionView(&gridA.desc), nullptr);
	registry.Unregister(StringCRC("temp"));
	EXPECT_EQ(registry.SelectionView(&gridA.desc), nullptr);
	EXPECT_EQ(registry.SelectionFor(nullptr), nullptr);
}

TEST_F(SelectionViewTest, CapacityIsEightGrids)
{
	std::vector<std::unique_ptr<TestGrid>> grids;
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxSelectionGrids; ++i)
	{
		grids.emplace_back(new TestGrid(2, 2));
		EXPECT_NE(registry.SelectionFor(&grids.back()->desc), nullptr) << i;
	}
	TestGrid ninth(2, 2);
	EXPECT_EQ(registry.SelectionFor(&ninth.desc), nullptr);
	EXPECT_EQ(registry.SelectionFor(&ninth.desc), nullptr);	// stays null (Warning logged once)
	EXPECT_NE(registry.SelectionFor(&grids[0]->desc), nullptr);	// existing entries still resolve
}

#ifdef DIA_DEBUG

namespace
{
	struct FakeDraw : public Dia::Core::IDebugDraw
	{
		Dia::Maths::Vector2D mouse;
		void RequestDraw(const Dia::Maths::Vector2D&, float, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
		void RequestDrawPoint(const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
		void RequestDrawRect(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDrawArc(const Dia::Maths::Vector2D&, float, float, float, Dia::Core::RGBA) override {}
		void RequestDrawRay(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, float, Dia::Core::RGBA) override {}
		void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDrawText(const Dia::Maths::Vector2D&, const char*, float, Dia::Core::RGBA) override {}
		void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
		void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
		void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
		void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
		void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, float, Dia::Core::RGBA) override {}
		void RequestDrawCellField(const Dia::Core::CellFieldDrawRequest&) override {}
		uint32_t DroppedCount() const override { return 0; }
		const Dia::Maths::Vector2D& GetMousePixel() const override { return mouse; }
	};

	const void* gSeenSelection = nullptr;
	uint32_t gDraws = 0;
	void RecordingDraw(Dia::Core::IDebugDraw&, const void* selection) { gSeenSelection = selection; ++gDraws; }

	struct SelectionViewTestViewer : SelectionViewTest
	{
		Dia::Graphics::CellFieldRegistry cellField;
		FakeDraw draw;
		std::unique_ptr<LayerUploader> uploader;
		std::unique_ptr<CellLayerViewerDomain> domain;

		void SetUp() override
		{
			SelectionViewTest::SetUp();
			gSeenSelection = reinterpret_cast<const void*>(&gDraws);	// sentinel: not null
			gDraws = 0;
			uploader.reset(new LayerUploader(cellField, nullptr));
			domain.reset(new CellLayerViewerDomain(registry, *uploader, nullptr));
		}
		void TearDown() override
		{
			domain.reset();
			uploader.reset();
		}
		void AddOverlay()
		{
			CellOverlayDesc o;
			o.id = StringCRC("probe");
			o.draw = fastdelegate::FastDelegate2<Dia::Core::IDebugDraw&, const void*>(&RecordingDraw);
			ASSERT_EQ(registry.RegisterOverlay(o), CellRegisterResult::Ok);
			ASSERT_TRUE(domain->ToggleOverlay(StringCRC("probe")));
		}
	};
}

TEST_F(SelectionViewTestViewer, ActiveSelectionFollowsActiveGrid)
{
	AddBoth();
	EXPECT_EQ(domain->ActiveSelectionView(), nullptr);	// before the first frame there is no active grid
	EXPECT_EQ(domain->ActiveSelection(), nullptr);
	domain->Update(draw);
	ASSERT_EQ(domain->ActiveGrid(), &gridA.desc);
	CellSelection* active = domain->ActiveSelection();
	ASSERT_NE(active, nullptr);
	EXPECT_EQ(active, registry.SelectionFor(&gridA.desc));
	Tools::SelectBox(*active, 0, 0, 1, 0);
	ASSERT_NE(domain->ActiveSelectionView(), nullptr);
	EXPECT_EQ(domain->ActiveSelectionView()->Count(), 2u);
}

TEST_F(SelectionViewTestViewer, ActiveGridSwitchClearsBothSelections)
{
	AddBoth();
	domain->Update(draw);
	registry.SelectionFor(&gridA.desc)->Set(1, true);
	registry.SelectionFor(&gridB.desc)->Set(2, true);
	registry.SelectionFor(&gridB.desc)->Set(3, true);

	ASSERT_TRUE(domain->SetBase(StringCRC("soil")));
	domain->Update(draw);
	ASSERT_EQ(domain->ActiveGrid(), &gridB.desc);
	EXPECT_EQ(registry.SelectionView(&gridA.desc)->Count(), 0u);
	ASSERT_NE(domain->ActiveSelectionView(), nullptr);
	EXPECT_EQ(domain->ActiveSelectionView()->Count(), 0u);
	EXPECT_EQ(domain->ActiveSelectionView(), registry.SelectionView(&gridB.desc));

	// Returning never resurrects an old selection.
	registry.SelectionFor(&gridB.desc)->Set(4, true);
	ASSERT_TRUE(domain->SetGrid(0));
	domain->Update(draw);
	EXPECT_EQ(domain->ActiveGrid(), &gridA.desc);
	EXPECT_EQ(domain->ActiveSelectionView()->Count(), 0u);
	EXPECT_EQ(registry.SelectionView(&gridB.desc)->Count(), 0u);
}

TEST_F(SelectionViewTestViewer, SwitchingToTheSameGridKeepsTheSelection)
{
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &gridA.desc)), CellRegisterResult::Ok);
	StubProvider rain;
	rain.values = std::vector<float>(12, 2.0f);
	ASSERT_EQ(registry.Register(rain.MakeDesc("rain", &gridA.desc)), CellRegisterResult::Ok);
	domain->Update(draw);
	registry.SelectionFor(&gridA.desc)->Set(5, true);
	ASSERT_TRUE(domain->SetBase(StringCRC("rain")));
	domain->Update(draw);
	EXPECT_EQ(domain->ActiveSelectionView()->Count(), 1u);
}

TEST_F(SelectionViewTestViewer, OverlayDrawReceivesTheActiveView)
{
	AddBoth();
	AddOverlay();
	domain->Update(draw);
	ASSERT_EQ(gDraws, 1u);
	ASSERT_NE(gSeenSelection, nullptr);
	const ICellSelectionView* active = domain->ActiveSelectionView();
	ASSERT_NE(active, nullptr);
	EXPECT_EQ(static_cast<const ICellSelectionView*>(gSeenSelection), active);

	// The overlay sees the commands' result.
	Tools::SelectBox(*domain->ActiveSelection(), 0, 0, 0, 2);
	domain->Update(draw);
	EXPECT_EQ(static_cast<const ICellSelectionView*>(gSeenSelection)->Count(), 3u);
}

TEST_F(SelectionViewTestViewer, OverlayDrawReceivesNullWithNoActiveGrid)
{
	AddOverlay();	// no layers registered: no active grid
	domain->Update(draw);
	EXPECT_EQ(gDraws, 1u);
	EXPECT_EQ(gSeenSelection, nullptr);
}

#endif // DIA_DEBUG
