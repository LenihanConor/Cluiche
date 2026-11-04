#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaPathfinding/HexTopology.h>

#include <string>

#ifdef DIA_DEBUG

// The hover-extras hook of CellLayerViewerDomain (AddHoverExtraFn): lines contributed by registered fns land in hover.extra.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	CellPickerView TestView() { CellPickerView v; v.viewportWidth = 1280; v.viewportHeight = 720; return v; }
	CellPickerView ViewFn0() { return TestView(); }

	struct Counting
	{
		uint32_t calls = 0;
		int32_t lastQ = -1, lastR = -1;
		void OnHover(const CellLayerViewerDomain::HoverInfo& h, CellLayerViewerDomain::HoverExtraLines& out)
		{
			++calls; lastQ = h.q; lastR = h.r;
			out.Add("Q", std::to_string(h.q).c_str());
			out.Add("R", std::to_string(h.r).c_str());
		}
	};
	struct Other
	{
		void OnHover(const CellLayerViewerDomain::HoverInfo&, CellLayerViewerDomain::HoverExtraLines& out) { out.Add("Other", "yes"); }
	};
	struct Silent
	{
		void OnHover(const CellLayerViewerDomain::HoverInfo&, CellLayerViewerDomain::HoverExtraLines&) {}
	};
	struct Long
	{
		void OnHover(const CellLayerViewerDomain::HoverInfo&, CellLayerViewerDomain::HoverExtraLines& out)
		{
			out.Add(std::string(100, 'L').c_str(), std::string(300, 'T').c_str());
		}
	};
	struct Flood
	{
		void OnHover(const CellLayerViewerDomain::HoverInfo&, CellLayerViewerDomain::HoverExtraLines& out)
		{
			for (int i = 0; i < 9; ++i) { out.Add("x", "y"); }
		}
	};

	typedef CellLayerViewerDomain::HoverExtraFn Fn;

	struct HoverExtraTest : ::testing::Test
	{
		TestGrid grid{ 4, 3 };
		CellFieldRegistry cellField;
		StubProvider temp;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr, CellLayerViewerDomain::ViewFn(&ViewFn0) };

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc)), CellRegisterResult::Ok);
		}
		void Point(int q, int r)
		{
			const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ q, r }, 1.0f);
			float sx, sy;
			WorldToScreen(TestView(), c.X(), c.Y(), sx, sy);
			draw.mouse = Dia::Maths::Vector2D(sx, sy);
		}
		Json::Value State() { Json::Value s; domain.GetJSONState(s); return s; }
	};
}

TEST_F(HoverExtraTest, NoFunctionMeansNoExtraKey)
{
	Point(2, 1);
	domain.Update(draw);
	const Json::Value s = State();
	ASSERT_TRUE(s["hover"]["valid"].asBool());
	EXPECT_FALSE(s["hover"].isMember("extra"));
}

TEST_F(HoverExtraTest, LinesAppearInOrderWithLabelAndText)
{
	Counting c;
	ASSERT_TRUE(domain.AddHoverExtraFn(Fn(&c, &Counting::OnHover)));
	Point(2, 1);
	domain.Update(draw);
	const Json::Value s = State();
	const Json::Value& extra = s["hover"]["extra"];
	ASSERT_TRUE(extra.isArray());
	ASSERT_EQ(extra.size(), 2u);
	EXPECT_EQ(extra[0u]["label"].asString(), "Q");
	EXPECT_EQ(extra[0u]["text"].asString(), "2");
	EXPECT_EQ(extra[1u]["label"].asString(), "R");
	EXPECT_EQ(extra[1u]["text"].asString(), "1");
	EXPECT_EQ(c.lastQ, 2);
	EXPECT_EQ(c.lastR, 1);
}

TEST_F(HoverExtraTest, FunctionAddingNothingLeavesNoKey)
{
	Silent s;
	ASSERT_TRUE(domain.AddHoverExtraFn(Fn(&s, &Silent::OnHover)));
	Point(2, 1);
	domain.Update(draw);
	EXPECT_FALSE(State()["hover"].isMember("extra"));
}

TEST_F(HoverExtraTest, TwoFunctionsBothContributeInRegistrationOrder)
{
	Counting c; Other o;
	ASSERT_TRUE(domain.AddHoverExtraFn(Fn(&o, &Other::OnHover)));
	ASSERT_TRUE(domain.AddHoverExtraFn(Fn(&c, &Counting::OnHover)));
	Point(1, 1);
	domain.Update(draw);
	const Json::Value extra = State()["hover"]["extra"];
	ASSERT_EQ(extra.size(), 3u);
	EXPECT_EQ(extra[0u]["label"].asString(), "Other");
	EXPECT_EQ(extra[1u]["label"].asString(), "Q");
	EXPECT_EQ(extra[2u]["label"].asString(), "R");
}

TEST_F(HoverExtraTest, NotCalledWithoutAValidHover)
{
	Counting c;
	ASSERT_TRUE(domain.AddHoverExtraFn(Fn(&c, &Counting::OnHover)));
	draw.mouse = Dia::Maths::Vector2D(5.0f, 5.0f);	// off the grid
	domain.Update(draw);
	const Json::Value s = State();
	EXPECT_FALSE(s["hover"]["valid"].asBool());
	EXPECT_FALSE(s["hover"].isMember("extra"));
	EXPECT_EQ(c.calls, 0u);
}

TEST_F(HoverExtraTest, FifthFunctionIsRejectedAndEmptyFunctionIsRejected)
{
	Counting a, b, c, d, e;
	EXPECT_FALSE(domain.AddHoverExtraFn(Fn()));
	EXPECT_TRUE(domain.AddHoverExtraFn(Fn(&a, &Counting::OnHover)));
	EXPECT_TRUE(domain.AddHoverExtraFn(Fn(&b, &Counting::OnHover)));
	EXPECT_TRUE(domain.AddHoverExtraFn(Fn(&c, &Counting::OnHover)));
	EXPECT_TRUE(domain.AddHoverExtraFn(Fn(&d, &Counting::OnHover)));
	EXPECT_FALSE(domain.AddHoverExtraFn(Fn(&e, &Counting::OnHover)));
}

TEST_F(HoverExtraTest, DuplicateAddIsRejected)
{
	Counting a;
	EXPECT_TRUE(domain.AddHoverExtraFn(Fn(&a, &Counting::OnHover)));
	EXPECT_FALSE(domain.AddHoverExtraFn(Fn(&a, &Counting::OnHover)));
	Point(2, 1);
	domain.Update(draw);
	EXPECT_EQ(State()["hover"]["extra"].size(), 2u) << "registered once, so called once";
	EXPECT_EQ(a.calls, 1u);
}

TEST_F(HoverExtraTest, RemoveStopsTheFunctionAndFreesTheSlot)
{
	Counting a, b, c, d, e;
	domain.AddHoverExtraFn(Fn(&a, &Counting::OnHover));
	domain.AddHoverExtraFn(Fn(&b, &Counting::OnHover));
	domain.AddHoverExtraFn(Fn(&c, &Counting::OnHover));
	domain.AddHoverExtraFn(Fn(&d, &Counting::OnHover));
	domain.RemoveHoverExtraFn(Fn(&b, &Counting::OnHover));
	EXPECT_TRUE(domain.AddHoverExtraFn(Fn(&e, &Counting::OnHover))) << "a slot was freed";
	Point(2, 1);
	domain.Update(draw);
	State();
	EXPECT_EQ(a.calls, 1u);
	EXPECT_EQ(b.calls, 0u);
	EXPECT_EQ(e.calls, 1u);
}

TEST_F(HoverExtraTest, RemovingAnUnknownFunctionIsANoOp)
{
	Counting a, other;
	domain.AddHoverExtraFn(Fn(&a, &Counting::OnHover));
	domain.RemoveHoverExtraFn(Fn(&other, &Counting::OnHover));
	domain.RemoveHoverExtraFn(Fn());
	Point(2, 1);
	domain.Update(draw);
	EXPECT_EQ(State()["hover"]["extra"].size(), 2u);
}

TEST_F(HoverExtraTest, RemovingTheLastFunctionRemovesTheKey)
{
	Counting a;
	domain.AddHoverExtraFn(Fn(&a, &Counting::OnHover));
	domain.RemoveHoverExtraFn(Fn(&a, &Counting::OnHover));
	Point(2, 1);
	domain.Update(draw);
	EXPECT_FALSE(State()["hover"].isMember("extra"));
}

TEST_F(HoverExtraTest, LongLabelAndTextAreTruncatedNotOverflowed)
{
	Long l;
	domain.AddHoverExtraFn(Fn(&l, &Long::OnHover));
	Point(2, 1);
	domain.Update(draw);
	const Json::Value extra = State()["hover"]["extra"];
	ASSERT_EQ(extra.size(), 1u);
	const std::string label = extra[0u]["label"].asString();
	const std::string text = extra[0u]["text"].asString();
	EXPECT_EQ(label.size(), 31u);
	EXPECT_EQ(text.size(), 63u);
	EXPECT_EQ(label, std::string(31, 'L'));
	EXPECT_EQ(text, std::string(63, 'T'));
}

TEST_F(HoverExtraTest, AFunctionCannotAddMoreThanFourLines)
{
	Flood f;
	domain.AddHoverExtraFn(Fn(&f, &Flood::OnHover));
	Point(2, 1);
	domain.Update(draw);
	EXPECT_EQ(State()["hover"]["extra"].size(), 4u);
}

TEST_F(HoverExtraTest, ExistingHoverFieldsAreUnchangedByTheHook)
{
	Point(2, 1);
	domain.Update(draw);
	const Json::Value before = State()["hover"];
	Counting c;
	domain.AddHoverExtraFn(Fn(&c, &Counting::OnHover));
	Json::Value after = State()["hover"];
	after.removeMember("extra");
	EXPECT_EQ(before.toStyledString(), after.toStyledString());
}

#endif // DIA_DEBUG
