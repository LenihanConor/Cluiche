#include <gtest/gtest.h>
#include <Modules/TestStages/CellLayersHoverOverride.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>

using namespace CluicheTest;

namespace
{
	Json::Value Params(const char* text)
	{
		Json::Value v;
		Json::Reader reader;
		EXPECT_TRUE(reader.parse(text, v));
		return v;
	}
}

TEST(CellLayersHoverOverrideTest, ParseXYActivatesOverride)
{
	HoverOverride o;
	std::string err;
	ASSERT_TRUE(ParseHoverParams(Params(R"({"x":12.5,"y":7})"), o, err)) << err;
	EXPECT_TRUE(o.active);
	EXPECT_FLOAT_EQ(o.pixel.X(), 12.5f);
	EXPECT_FLOAT_EQ(o.pixel.Y(), 7.0f);
}

TEST(CellLayersHoverOverrideTest, ParseClearDeactivatesOverride)
{
	HoverOverride o;
	o.active = true;
	std::string err;
	ASSERT_TRUE(ParseHoverParams(Params(R"({"clear":true})"), o, err)) << err;
	EXPECT_FALSE(o.active);
}

TEST(CellLayersHoverOverrideTest, ParseRejectsBadParamsAndLeavesOverrideUntouched)
{
	struct Case { const char* json; const char* text; };
	const Case cases[] = {
		{ R"(5)", "params must be an object" },
		{ R"({})", "missing or non-numeric 'x'" },
		{ R"({"x":"a","y":1})", "missing or non-numeric 'x'" },
		{ R"({"x":1})", "missing or non-numeric 'y'" },
		{ R"({"x":1,"y":null})", "missing or non-numeric 'y'" },
		{ R"({"clear":1})", "non-bool param 'clear'" },
		{ R"({"clear":true,"x":1,"y":2})", "'clear' conflicts with x/y" },
	};
	for (const Case& c : cases)
	{
		HoverOverride o;
		o.active = true;
		o.pixel = Dia::Maths::Vector2D(3.0f, 4.0f);
		std::string err;
		EXPECT_FALSE(ParseHoverParams(Params(c.json), o, err)) << c.json;
		EXPECT_NE(err.find(c.text), std::string::npos) << c.json << " -> " << err;
		EXPECT_TRUE(o.active) << c.json;
		EXPECT_FLOAT_EQ(o.pixel.X(), 3.0f) << c.json;
		EXPECT_FLOAT_EQ(o.pixel.Y(), 4.0f) << c.json;
	}
}

TEST(CellLayersHoverOverrideTest, ClearFalseWithXYIsAnOverride)
{
	HoverOverride o;
	std::string err;
	ASSERT_TRUE(ParseHoverParams(Params(R"({"clear":false,"x":1,"y":2})"), o, err)) << err;
	EXPECT_TRUE(o.active);
}

TEST(CellLayersHoverOverrideTest, DrawReportsRealMouseWhenInactive)
{
	Dia::CellLayers::Testing::FakeDebugDraw inner;
	inner.mouse = Dia::Maths::Vector2D(100.0f, 200.0f);
	HoverOverride o;
	HoverOverrideDraw draw(inner, o);
	EXPECT_FLOAT_EQ(draw.GetMousePixel().X(), 100.0f);
	EXPECT_FLOAT_EQ(draw.GetMousePixel().Y(), 200.0f);
}

TEST(CellLayersHoverOverrideTest, DrawReportsOverrideWhenActiveAndFollowsChanges)
{
	Dia::CellLayers::Testing::FakeDebugDraw inner;
	inner.mouse = Dia::Maths::Vector2D(100.0f, 200.0f);
	HoverOverride o;
	o.active = true;
	o.pixel = Dia::Maths::Vector2D(5.0f, 6.0f);
	HoverOverrideDraw draw(inner, o);
	EXPECT_FLOAT_EQ(draw.GetMousePixel().X(), 5.0f);
	o.pixel = Dia::Maths::Vector2D(7.0f, 8.0f);
	EXPECT_FLOAT_EQ(draw.GetMousePixel().X(), 7.0f);
	o.active = false;
	EXPECT_FLOAT_EQ(draw.GetMousePixel().X(), 100.0f);
}

TEST(CellLayersHoverOverrideTest, DrawForwardsDrawCallsAndDroppedCount)
{
	Dia::CellLayers::Testing::FakeDebugDraw inner;
	HoverOverride o;
	HoverOverrideDraw draw(inner, o);
	draw.RequestDraw(Dia::Maths::Vector2D(0, 0), Dia::Maths::Vector2D(1, 1), Dia::Core::RGBA(1, 2, 3, 4));
	Dia::Core::CellFieldDrawRequest req;
	draw.RequestDrawCellField(req);
	EXPECT_EQ(inner.lines.size(), 1u);
	EXPECT_EQ(inner.cellFieldRequests, 1u);
	EXPECT_EQ(draw.DroppedCount(), 0u);
}

// ---- pointer_frames carries the hover override (no extra sim-hosted name) ----
#include <Modules/TestStages/CellLayersPointerQueue.h>

TEST(CellLayersPointerInjectionTest, FramesOnlyBehavesAsBefore)
{
	PointerFrameQueue q;
	HoverOverride h;
	const Json::Value out = ApplyPointerInjection(Params(R"({"frames":[{"x":1,"y":2},{"x":3,"y":4}]})"), q, h);
	EXPECT_FALSE(out.isMember("error")) << out.toStyledString();
	EXPECT_EQ(out["queued"].asUInt(), 2u);
	EXPECT_EQ(q.Pending(), 2u);
	EXPECT_FALSE(h.active);
	EXPECT_FALSE(out.isMember("hover"));
}

TEST(CellLayersPointerInjectionTest, FramesAreStillRequiredWithoutHover)
{
	PointerFrameQueue q;
	HoverOverride h;
	EXPECT_EQ(ApplyPointerInjection(Params(R"({})"), q, h)["error"].asString(), "missing or non-array param 'frames'");
	EXPECT_EQ(ApplyPointerInjection(Params(R"({"frames":[]})"), q, h)["error"].asString(), "param 'frames' is empty");
	EXPECT_EQ(q.Pending(), 0u);
}

TEST(CellLayersPointerInjectionTest, HoverAloneSetsOverrideAndQueuesNothing)
{
	PointerFrameQueue q;
	HoverOverride h;
	const Json::Value out = ApplyPointerInjection(Params(R"({"hover":{"x":10,"y":20}})"), q, h);
	EXPECT_FALSE(out.isMember("error")) << out.toStyledString();
	EXPECT_TRUE(h.active);
	EXPECT_FLOAT_EQ(h.pixel.X(), 10.0f);
	EXPECT_FLOAT_EQ(h.pixel.Y(), 20.0f);
	EXPECT_EQ(out["queued"].asUInt(), 0u);
	EXPECT_TRUE(out["hover"].asBool());
	EXPECT_EQ(q.Pending(), 0u);
}

TEST(CellLayersPointerInjectionTest, HoverClearDeactivates)
{
	PointerFrameQueue q;
	HoverOverride h;
	h.active = true;
	const Json::Value out = ApplyPointerInjection(Params(R"({"hover":{"clear":true}})"), q, h);
	EXPECT_FALSE(out.isMember("error"));
	EXPECT_FALSE(h.active);
	EXPECT_FALSE(out["hover"].asBool());
}

TEST(CellLayersPointerInjectionTest, HoverAndFramesApplyTogether)
{
	PointerFrameQueue q;
	HoverOverride h;
	const Json::Value out = ApplyPointerInjection(Params(R"({"hover":{"x":1,"y":2},"frames":[{"x":5,"y":6}]})"), q, h);
	EXPECT_FALSE(out.isMember("error"));
	EXPECT_TRUE(h.active);
	EXPECT_EQ(q.Pending(), 1u);
	EXPECT_EQ(out["queued"].asUInt(), 1u);
}

TEST(CellLayersPointerInjectionTest, BadHoverRejectsEverythingAndChangesNothing)
{
	PointerFrameQueue q;
	HoverOverride h;
	h.active = true;
	h.pixel = Dia::Maths::Vector2D(3.0f, 4.0f);
	const Json::Value out = ApplyPointerInjection(Params(R"({"hover":{"x":1},"frames":[{"x":5,"y":6}]})"), q, h);
	EXPECT_NE(out["error"].asString().find("hover: missing or non-numeric 'y'"), std::string::npos) << out.toStyledString();
	EXPECT_EQ(q.Pending(), 0u);
	EXPECT_TRUE(h.active);
	EXPECT_FLOAT_EQ(h.pixel.X(), 3.0f);
}

TEST(CellLayersPointerInjectionTest, BadFramesDoNotApplyHover)
{
	PointerFrameQueue q;
	HoverOverride h;
	const Json::Value out = ApplyPointerInjection(Params(R"({"hover":{"x":1,"y":2},"frames":[{"x":5}]})"), q, h);
	EXPECT_NE(out["error"].asString().find("frame 0"), std::string::npos) << out.toStyledString();
	EXPECT_FALSE(h.active);
	EXPECT_EQ(q.Pending(), 0u);
}

TEST(CellLayersPointerInjectionTest, FullQueueRejectsAndDoesNotApplyHover)
{
	PointerFrameQueue q;
	HoverOverride h;
	for (int i = 0; i < 2; ++i)   // 2 x 256 fills half; fill the rest below
	{
		std::string f = R"({"frames":[)";
		for (int k = 0; k < 256; ++k) f += std::string(k ? "," : "") + R"({"x":0,"y":0})";
		ASSERT_FALSE(ApplyPointerInjection(Params((f + "]}").c_str()), q, h).isMember("error"));
	}
	ASSERT_EQ(q.Pending(), 512u);
	const Json::Value out = ApplyPointerInjection(Params(R"({"hover":{"x":1,"y":2},"frames":[{"x":0,"y":0}]})"), q, h);
	EXPECT_EQ(out["error"].asString(), "queue full");
	EXPECT_FALSE(h.active);
}

TEST(CellLayersPointerInjectionTest, HoverNotAnObjectIsRejected)
{
	PointerFrameQueue q;
	HoverOverride h;
	EXPECT_NE(ApplyPointerInjection(Params(R"({"hover":5})"), q, h)["error"].asString().find("hover: params must be an object"), std::string::npos);
	EXPECT_FALSE(h.active);
}
