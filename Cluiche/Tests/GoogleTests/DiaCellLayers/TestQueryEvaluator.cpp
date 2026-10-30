#include <gtest/gtest.h>
#include <DiaCellLayers/QueryEvaluator.h>
#include <DiaCellLayers/Testing/QueryTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#include <math.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;

namespace
{
	using Cells = std::vector<uint32_t>;
	const float kNaN = std::numeric_limits<float>::quiet_NaN();

	// 8x8 grid with a (ramp), b (scrambled), c (category cycling Grass/Water/Rock/Sand).
	struct QueryEvaluatorTest : ::testing::Test
	{
		QueryEvalFixture	f{ 8, 8 };
		std::vector<float>	a, b, c;

		void SetUp() override
		{
			for (uint32_t i = 0; i < 64; ++i)
			{
				a.push_back(static_cast<float>(i) / 64.0f);
				b.push_back(static_cast<float>((i * 7u) % 64u) / 64.0f);
				c.push_back(static_cast<float>(i % 4u));
			}
			f.PublishSchema({ "a", "b" }, { "c" });
			ASSERT_TRUE(f.AddValues("a", a));
			ASSERT_TRUE(f.AddValues("b", b));
			ASSERT_TRUE(f.AddValues("c", c, CellValueKind::Category));
		}

		void Expect(const char* text, const Cells& expected)
		{
			ASSERT_TRUE(f.Eval(text)) << text << " : " << f.Msg();
			EXPECT_EQ(expected, f.Result()) << text;
			EXPECT_EQ(static_cast<uint32_t>(expected.size()), f.evaluator.Count()) << text;
		}
	};

	// Small grid fixture with explicit values on layer "x".
	struct SmallEval
	{
		QueryEvalFixture f;
		SmallEval(uint16_t w, uint16_t h, std::vector<float> x) : f(w, h)
		{
			f.PublishSchema({ "x" });
			EXPECT_TRUE(f.AddValues("x", std::move(x)));
		}
	};
}

TEST(QueryEvaluatorOps, EachFloatOperator)
{
	SmallEval s(5, 1, { 0.1f, 0.2f, 0.5f, 0.7f, 0.9f });
	auto run = [&](const char* t) { EXPECT_TRUE(s.f.Eval(t)) << t << s.f.Msg(); return s.f.Result(); };
	EXPECT_EQ((Cells{ 0, 1 }), run("x < 0.5"));
	EXPECT_EQ((Cells{ 0, 1, 2 }), run("x <= 0.5"));
	EXPECT_EQ((Cells{ 3, 4 }), run("x > 0.5"));
	EXPECT_EQ((Cells{ 2, 3, 4 }), run("x >= 0.5"));
	EXPECT_EQ((Cells{ 2 }), run("x == 0.5"));
	EXPECT_EQ((Cells{ 2 }), run("x = 0.5"));
	EXPECT_EQ((Cells{ 0, 1, 3, 4 }), run("x != 0.5"));
}

TEST(QueryEvaluatorOps, ThresholdNarrowedToFloatMatchesStoredFloat)
{
	SmallEval s(3, 1, { 0.1f, 0.2f, 0.3f });
	ASSERT_TRUE(s.f.Eval("x <= 0.2"));
	EXPECT_EQ((Cells{ 0, 1 }), s.f.Result());
	ASSERT_TRUE(s.f.Eval("x == 0.2"));
	EXPECT_EQ((Cells{ 1 }), s.f.Result());
	ASSERT_TRUE(s.f.Eval("x >= 0.2"));
	EXPECT_EQ((Cells{ 1, 2 }), s.f.Result());
	ASSERT_TRUE(s.f.Eval("x < 0.2"));
	EXPECT_EQ((Cells{ 0 }), s.f.Result());
	ASSERT_TRUE(s.f.Eval("x > 0.2"));
	EXPECT_EQ((Cells{ 2 }), s.f.Result());
}

TEST(QueryEvaluatorOps, NaNIsFalseForEveryOperator)
{
	SmallEval s(2, 1, { kNaN, 0.5f });
	const char* ops[] = { "x < 0.5", "x <= 0.5", "x > 0.5", "x >= 0.5", "x == 0.5", "x = 0.5", "x != 0.5" };
	for (const char* t : ops)
	{
		ASSERT_TRUE(s.f.Eval(t)) << t;
		EXPECT_FALSE(s.f.evaluator.Test(0)) << t;
	}
	ASSERT_TRUE(s.f.Eval("x != 1"));
	EXPECT_EQ((Cells{ 1 }), s.f.Result());
}

TEST(QueryEvaluatorOps, NotOfFalseCompareIsTrueOnNaN)
{
	SmallEval s(2, 1, { kNaN, 0.9f });
	ASSERT_TRUE(s.f.Eval("not x < 0.5"));
	EXPECT_EQ((Cells{ 0, 1 }), s.f.Result());	// documented: complement of false
}

TEST_F(QueryEvaluatorTest, CategoryEqualsAndNotEquals)
{
	Expect("c = Rock", ExpectedCells(64, [](uint32_t i) { return i % 4 == 2; }));
	Expect("c == Water", ExpectedCells(64, [](uint32_t i) { return i % 4 == 1; }));
	Expect("c != Grass", ExpectedCells(64, [](uint32_t i) { return i % 4 != 0; }));
}

TEST(QueryEvaluatorOps, CategoryRoundsAndNaNNeverMatches)
{
	QueryEvalFixture f(4, 1);
	f.PublishSchema({}, { "c" });
	ASSERT_TRUE(f.AddValues("c", { 1.4f, 1.6f, kNaN, 2.0f }, CellValueKind::Category));
	ASSERT_TRUE(f.Eval("c = Water"));	// floor(v+0.5)==1 -> only 1.4
	EXPECT_EQ((Cells{ 0 }), f.Result());
	ASSERT_TRUE(f.Eval("c != Water"));	// NaN excluded
	EXPECT_EQ((Cells{ 1, 3 }), f.Result());
}

TEST_F(QueryEvaluatorTest, PrecedenceTable)
{
	auto A = [&](uint32_t i) { return a[i]; };
	auto B = [&](uint32_t i) { return b[i]; };
	auto C = [&](uint32_t i) { return static_cast<uint32_t>(c[i]); };
	// and binds tighter than or
	Expect("a < 0.5 or b > 0.5 and c = Rock", ExpectedCells(64, [&](uint32_t i) { return A(i) < 0.5f || (B(i) > 0.5f && C(i) == 2); }));
	// not binds tighter than and
	Expect("not a < 0.5 and b < 0.5", ExpectedCells(64, [&](uint32_t i) { return !(A(i) < 0.5f) && B(i) < 0.5f; }));
	// parentheses override
	Expect("(a < 0.5 or b > 0.5) and c = Rock", ExpectedCells(64, [&](uint32_t i) { return (A(i) < 0.5f || B(i) > 0.5f) && C(i) == 2; }));
	Expect("not (a < 0.5 and b < 0.5)", ExpectedCells(64, [&](uint32_t i) { return !(A(i) < 0.5f && B(i) < 0.5f); }));
	// double negation
	Expect("not not a < 0.25", ExpectedCells(64, [&](uint32_t i) { return A(i) < 0.25f; }));
	Expect("not not not a < 0.25", ExpectedCells(64, [&](uint32_t i) { return !(A(i) < 0.25f); }));
	// left associative chains
	Expect("a < 0.5 and b < 0.5 and c != Grass", ExpectedCells(64, [&](uint32_t i) { return A(i) < 0.5f && B(i) < 0.5f && C(i) != 0; }));
	Expect("a > 0.9 or b > 0.9 or c = Sand", ExpectedCells(64, [&](uint32_t i) { return A(i) > 0.9f || B(i) > 0.9f || C(i) == 3; }));
	// right-nested groups use deeper temp slots
	Expect("a < 0.9 and (b < 0.9 and (c != Grass and (a > 0.1 or b < 0.1)))",
		ExpectedCells(64, [&](uint32_t i) { return A(i) < 0.9f && (B(i) < 0.9f && (C(i) != 0 && (A(i) > 0.1f || B(i) < 0.1f))); }));
	// same layer on both sides
	Expect("a >= 0.25 and a < 0.5", ExpectedCells(64, [&](uint32_t i) { return A(i) >= 0.25f && A(i) < 0.5f; }));
}

TEST(QueryEvaluatorNot, MasksTailBitsForNonMultipleOf64)
{
	for (uint32_t n : { 70u, 6656u, 65u, 63u })
	{
		const uint16_t w = static_cast<uint16_t>(n);
		SmallEval s(w, 1, std::vector<float>(n, 0.9f));	// x < 0.5 false everywhere
		ASSERT_TRUE(s.f.Eval("not x < 0.5")) << n;
		EXPECT_EQ(n, s.f.evaluator.Count()) << n;
		const uint32_t words = (n + 63) / 64;
		const uint32_t tail = n % 64;
		if (tail != 0)
		{
			EXPECT_EQ(0u, s.f.evaluator.Words()[words - 1] >> tail) << n;
		}
		EXPECT_FALSE(s.f.evaluator.Test(n));	// out of range
	}
}

TEST(QueryEvaluatorNot, NotNotRestoresOriginalAt70Cells)
{
	std::vector<float> v(70);
	for (uint32_t i = 0; i < 70; ++i) { v[i] = (i % 3 == 0) ? 0.1f : 0.9f; }
	SmallEval s(70, 1, v);
	ASSERT_TRUE(s.f.Eval("not not x < 0.5"));
	EXPECT_EQ(24u, s.f.evaluator.Count());
}

TEST_F(QueryEvaluatorTest, ReadsEachLayerOncePerCompare)
{
	ASSERT_TRUE(f.Eval("a < 0.5"));
	EXPECT_EQ(1u, f.providers[0].readCalls);
	ASSERT_TRUE(f.Eval("a < 0.7 and a > 0.2"));
	EXPECT_EQ(1u, f.providers[0].readCalls);	// cached by LayerReader (version unchanged)
}

TEST_F(QueryEvaluatorTest, RepeatedEvaluationReusesResultsAndResizes)
{
	Expect("a < 0.5", ExpectedCells(64, [&](uint32_t i) { return a[i] < 0.5f; }));
	Expect("a < 0.25", ExpectedCells(64, [&](uint32_t i) { return a[i] < 0.25f; }));
}

TEST_F(QueryEvaluatorTest, ApplyToReplacesSelectionAndBumpsOnce)
{
	CellSelection sel;
	sel.Reset(&f.q.gridA);
	sel.Set(63, true);	// stale member that must be removed
	const uint32_t v0 = sel.Version();
	ASSERT_TRUE(f.Eval("c = Rock"));
	ASSERT_TRUE(f.evaluator.ApplyTo(sel));
	EXPECT_EQ(ExpectedCells(64, [](uint32_t i) { return i % 4 == 2; }), ToVector(sel));
	EXPECT_EQ(v0 + 1, sel.Version());
	// Same result again: no bump.
	ASSERT_TRUE(f.evaluator.ApplyTo(sel));
	EXPECT_EQ(v0 + 1, sel.Version());
	// A different result: exactly one more bump.
	ASSERT_TRUE(f.Eval("c = Sand"));
	ASSERT_TRUE(f.evaluator.ApplyTo(sel));
	EXPECT_EQ(v0 + 2, sel.Version());
}

TEST_F(QueryEvaluatorTest, ApplyToRejectsMismatchedSelection)
{
	CellSelection sel;
	sel.Reset(&f.q.gridB);	// 4x4
	ASSERT_TRUE(f.Eval("a < 0.5"));
	EXPECT_FALSE(f.evaluator.ApplyTo(sel));
}

TEST(QueryEvaluatorErrors, NoDataLayerFailsWithLocatedError)
{
	QueryEvalFixture f(4, 1);
	f.PublishSchema({ "x" });
	ASSERT_TRUE(f.AddValues("x", { 1, 2, 3, 4 }));
	StubViewValueSource source;
	source.live = false;
	source.hasData = false;
	f.q.registry.SetViewValueSource(&source);
	EXPECT_FALSE(f.Eval("  x < 0.5"));
	EXPECT_NE(std::string::npos, f.Msg().find("no data for layer"));
	EXPECT_EQ(2u, f.evaluator.Error().mColumn);
	EXPECT_EQ(1u, f.evaluator.Error().mLength);
	EXPECT_EQ(nullptr, f.evaluator.Words());
	EXPECT_EQ(0u, f.evaluator.Count());
}

TEST(QueryEvaluatorErrors, CountMismatchFails)
{
	QueryEvalFixture f(4, 1);
	f.PublishSchema({ "x" });
	ASSERT_TRUE(f.AddValues("x", { 1, 2, 3, 4 }));
	ASSERT_TRUE(f.q.Run("x < 3", f.parser, f.resolver));
	EXPECT_FALSE(f.evaluator.Evaluate(f.parser, f.resolver, f.Reader(), 9));
	EXPECT_NE(std::string(), std::string(f.evaluator.Error().mMessage));
}

TEST(QueryEvaluatorViews, PastViewEvaluatesAgainstScrubbedValues)
{
	QueryEvalFixture f(4, 1);
	f.PublishSchema({ "x" });
	ASSERT_TRUE(f.AddValues("x", { 0.9f, 0.9f, 0.9f, 0.9f }));	// live values would match nothing
	StubViewValueSource source;
	source.live = false;
	source.values = { 0.1f, 0.9f, 0.2f, 0.8f };
	f.q.registry.SetViewValueSource(&source);
	ASSERT_TRUE(f.Eval("x < 0.5")) << f.Msg();
	EXPECT_EQ((Cells{ 0, 2 }), f.Result());
}
