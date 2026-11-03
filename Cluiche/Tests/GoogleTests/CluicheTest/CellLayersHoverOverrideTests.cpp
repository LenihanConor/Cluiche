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
