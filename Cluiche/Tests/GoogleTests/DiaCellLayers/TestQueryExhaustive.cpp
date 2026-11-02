#include <gtest/gtest.h>
#include <DiaCellLayers/QueryEvaluator.h>
#include <DiaCellLayers/QueryLexer.h>
#include <DiaCellLayers/QueryParser.h>
#include <DiaCellLayers/Testing/QueryTestSupport.h>

#include <algorithm>
#include <limits>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

// Feature 3 (Query) exhaustive pass 2: gaps left by the first pass. Keyword case at parse level, operator typos, non-ASCII input,
// resolver error spans on category layers, evaluator state before/after evaluation, determinism, the densest 512-character query,
// documented float narrowing, and a seeded randomized differential test of the evaluator against a per-cell reference interpreter.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;

namespace
{
	using Cells = std::vector<uint32_t>;
	const float kNaN = std::numeric_limits<float>::quiet_NaN();

	struct SmallEval
	{
		QueryEvalFixture f;
		SmallEval(uint16_t w, uint16_t h, std::vector<float> x) : f(w, h)
		{
			f.PublishSchema({ "x" });
			EXPECT_TRUE(f.AddValues("x", std::move(x)));
		}
	};

	struct Lcg
	{
		uint32_t s;
		uint32_t Next() { s = s * 1664525u + 1013904223u; return s >> 8; }
		uint32_t Below(uint32_t n) { return Next() % n; }
	};
}

// ---- Lexer / parser ----

TEST(QueryExhaustiveParse, UppercaseKeywordsAreIdentifiersAndFailAtTheirColumn)
{
	QueryParser p;
	ASSERT_FALSE(p.Parse("a < 1 AND b < 2"));
	EXPECT_EQ(6u, p.Error().mColumn);
	EXPECT_EQ(3u, p.Error().mLength);
	ASSERT_FALSE(p.Parse("a < 1 Or b < 2"));
	EXPECT_EQ(6u, p.Error().mColumn);
	EXPECT_EQ(2u, p.Error().mLength);
	ASSERT_FALSE(p.Parse("NOT a < 1"));	// "NOT" is a name; the next token is not an operator
	EXPECT_EQ(4u, p.Error().mColumn);
	EXPECT_EQ(1u, p.Error().mLength);
	EXPECT_EQ(QueryNode::kNone, p.RootIndex());
	EXPECT_EQ(0u, p.NodeCount());
}

TEST(QueryExhaustiveParse, NumberOnTheLeftIsUnexpected)
{
	QueryParser p;
	ASSERT_FALSE(p.Parse("1 < a"));
	EXPECT_EQ(0u, p.Error().mColumn);
	EXPECT_EQ(1u, p.Error().mLength);
	ASSERT_FALSE(p.Parse("a < 1 and 2 > b"));
	EXPECT_EQ(10u, p.Error().mColumn);
}

TEST(QueryExhaustiveParse, OperatorTyposAreLocatedAndNeverParse)
{
	struct Case { const char* text; uint32_t column; };
	const Case cases[] = {
		{ "a <> 1", 3 },	// "<" then ">" where a number is expected
		{ "a =< 1", 3 },	// "=" then "<"
		{ "a => 1", 3 },	// "=" then ">"
		{ "a === 1", 4 },	// "==" then "="
		{ "a !== 1", 4 },	// "!=" then "="
		{ "a << 1", 3 },
		{ "a >= = 1", 5 },
	};
	for (const Case& c : cases)
	{
		QueryParser p;
		ASSERT_FALSE(p.Parse(c.text)) << c.text;
		EXPECT_EQ(c.column, p.Error().mColumn) << c.text;
		EXPECT_NE('\0', p.Error().mMessage[0]) << c.text;
	}
}

TEST(QueryExhaustiveParse, NonAsciiBytesAreUnexpectedCharactersNotIdentifiers)
{
	QueryLexer lx;
	ASSERT_FALSE(lx.Tokenize("a < 1 and \xC3\xA9 > 2"));
	EXPECT_EQ(10u, lx.Error().mColumn);
	EXPECT_EQ(1u, lx.Error().mLength);
	EXPECT_EQ(0u, lx.TokenCount());
	ASSERT_FALSE(lx.Tokenize("\xFF"));
	EXPECT_EQ(0u, lx.Error().mColumn);
	ASSERT_FALSE(lx.Tokenize("a # 1"));
	EXPECT_EQ(2u, lx.Error().mColumn);
	ASSERT_FALSE(lx.Tokenize("a < 1;"));
	EXPECT_EQ(5u, lx.Error().mColumn);
}

TEST(QueryExhaustiveParse, EmbeddedNulEndsTheText)
{
	QueryParser p;
	const char text[] = "a < 1\0 garbage (((";
	ASSERT_TRUE(p.Parse(text));	// the API takes a null-terminated string
	EXPECT_EQ(1u, p.NodeCount());
}

TEST(QueryExhaustiveParse, DensestFiveTwelveCharacterQueryFitsTheNodePool)
{
	// "(a<1)or(a<1)or..." packs two nodes into seven characters, the most node-dense text that can lex.
	std::string text = "(a<1)";
	while (text.size() + 7 <= QueryLexer::kMaxInput) { text += "or(a<1)"; }
	ASSERT_LE(text.size(), QueryLexer::kMaxInput);
	QueryParser p;
	ASSERT_TRUE(p.Parse(text.c_str())) << p.Error().mMessage;
	EXPECT_LE(p.NodeCount(), QueryParser::kMaxNodes);
	EXPECT_GE(p.NodeCount(), 140u);
	// and the same text through the evaluator: every cell with x<1 once, left-leaning chain uses two pool slots.
	SmallEval s(3, 1, { 0.0f, 2.0f, kNaN });
	std::string onX = text;
	for (char& ch : onX) { if (ch == 'a') { ch = 'x'; } }
	ASSERT_TRUE(s.f.Eval(onX.c_str())) << s.f.Msg();
	EXPECT_EQ((Cells{ 0 }), s.f.Result());
}

TEST(QueryExhaustiveParse, NodeCapacityIsNeverReachableFromFiveTwelveCharacters)
{
	// Denser variants: "a=b" enum comparisons and stacked nots inside the depth limit.
	std::string text = "(a=b)";
	while (text.size() + 7 <= QueryLexer::kMaxInput) { text += "or(a=b)"; }
	QueryParser p;
	ASSERT_TRUE(p.Parse(text.c_str())) << p.Error().mMessage;
	EXPECT_LT(p.NodeCount(), QueryParser::kMaxNodes);
}

TEST(QueryExhaustiveParse, DefaultErrorHasNoSuggestionAndEmptyMessage)
{
	QueryError e;
	EXPECT_FALSE(e.HasSuggestion());
	EXPECT_EQ('\0', e.mMessage[0]);
	EXPECT_EQ(0u, e.mColumn);
	EXPECT_EQ(0u, e.mLength);
	QueryParser p;
	ASSERT_TRUE(p.Parse("a < 1"));
	EXPECT_EQ('\0', p.Error().mMessage[0]);	// success clears a previous error
	ASSERT_FALSE(p.Parse("a <"));
	ASSERT_TRUE(p.Parse("a < 1"));
	EXPECT_EQ('\0', p.Error().mMessage[0]);
}

// ---- Resolver ----

namespace
{
	struct ResolverRig
	{
		QueryFixture q;
		QueryParser parser;
		QueryResolver resolver;
		ResolverRig()
		{
			q.fx.Add(QueryFixture::Category("terrain", { "grass", "water", "rock" }));
			q.fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			q.Publish();
			q.AddLayer("terrain", &q.gridA, "cells/terrain", CellValueKind::Category);
			q.AddLayer("temp", &q.gridA, "cells/temp");
		}
		bool Run(const char* t) { return q.Run(t, parser, resolver); }
	};
}

TEST(QueryExhaustiveResolve, CategoryNotEqualErrorsMirrorEqualErrors)
{
	ResolverRig r;
	ASSERT_FALSE(r.Run("terrain != Water"));	// wrong case
	EXPECT_EQ(11u, r.resolver.Error().mColumn);
	EXPECT_EQ(5u, r.resolver.Error().mLength);
	EXPECT_STREQ("water", r.resolver.Error().mSuggestion);
	ASSERT_FALSE(r.Run("terrain != lava"));	// unknown, no close match
	EXPECT_FALSE(r.resolver.Error().HasSuggestion());
	ASSERT_FALSE(r.Run("terrain != 2"));
	EXPECT_EQ(11u, r.resolver.Error().mColumn);
	ASSERT_FALSE(r.Run("terrain >= water"));	// parser rejects before resolve; resolver never sees an identifier on an ordering op
}

TEST(QueryExhaustiveResolve, SuggestionThresholdIsSixTenthsNotFiveTenths)
{
	// Mutation finding: the first-pass boundary test used ratio 0.4, which does not distinguish 0.6 from 0.5. Ratios just under
	// 0.6 (7/12 = 0.583) and exactly 0.5 (3/6) must NOT be suggested; 0.6 (3/5) and 0.667 (4/12 distance) must.
	QueryFixture q;
	q.fx.Add(UploaderSchemaFixture::FloatEntry("abcdefghijkl", 0, 1));
	q.fx.Add(QueryFixture::Category("terrain", { "abcdefghijkl" }));
	q.Publish();
	ASSERT_TRUE(q.AddLayer("lay", &q.gridA, "cells/abcdefghijkl"));
	ASSERT_TRUE(q.AddLayer("terrain", &q.gridA, "cells/terrain", CellValueKind::Category));
	QueryParser parser;
	QueryResolver resolver;
	ASSERT_FALSE(q.Run("abcdefgxxxxx > 1", parser, resolver));	// distance 5 of 12 = 0.583
	EXPECT_FALSE(resolver.Error().HasSuggestion());
	ASSERT_FALSE(q.Run("abcdefghxxxx > 1", parser, resolver));	// distance 4 of 12 = 0.667
	EXPECT_STREQ("abcdefghijkl", resolver.Error().mSuggestion);
	ASSERT_FALSE(q.Run("terrain = abcdefgxxxxx", parser, resolver));
	EXPECT_FALSE(resolver.Error().HasSuggestion());
	ASSERT_FALSE(q.Run("terrain = abcdefghxxxx", parser, resolver));
	EXPECT_STREQ("abcdefghijkl", resolver.Error().mSuggestion);
}

TEST(QueryExhaustiveResolve, SuggestionAtExactlyHalfIsNotOffered)
{
	QueryFixture q;
	q.fx.Add(UploaderSchemaFixture::FloatEntry("abcdef", 0, 1));
	q.Publish();
	ASSERT_TRUE(q.AddLayer("lay", &q.gridA, "cells/abcdef"));
	QueryParser parser;
	QueryResolver resolver;
	ASSERT_FALSE(q.Run("abcxxx > 1", parser, resolver));	// distance 3 of 6 = 0.5
	EXPECT_FALSE(resolver.Error().HasSuggestion());
}

TEST(QueryExhaustiveResolve, NumericOperatorOnCategoryPointsAtTheOperator)
{
	ResolverRig r;
	for (const char* op : { "<", "<=", ">", ">=" })
	{
		const std::string t = std::string("terrain ") + op + " 1";
		ASSERT_FALSE(r.Run(t.c_str())) << t;
		EXPECT_EQ(8u, r.resolver.Error().mColumn) << t;
		EXPECT_EQ(static_cast<uint32_t>(strlen(op)), r.resolver.Error().mLength) << t;
	}
}

TEST(QueryExhaustiveResolve, SecondBadComparisonInACompoundReportsItsOwnSpan)
{
	ResolverRig r;
	ASSERT_FALSE(r.Run("temp > 1 and (terrain = grass or terrian = rock)"));
	EXPECT_EQ(33u, r.resolver.Error().mColumn);
	EXPECT_EQ(7u, r.resolver.Error().mLength);
	EXPECT_STREQ("terrain", r.resolver.Error().mSuggestion);
}

TEST(QueryExhaustiveResolve, BindingOutOfRangeOrOnLogicalNodesIsUnbound)
{
	ResolverRig r;
	ASSERT_TRUE(r.Run("temp > 1 and not temp < 5"));
	EXPECT_FALSE(r.resolver.Binding(QueryResolver::kMaxBindings).mBound);
	EXPECT_FALSE(r.resolver.Binding(0xFFFFFFFFu).mBound);
	EXPECT_FALSE(r.resolver.Binding(QueryNode::kNone).mBound);
	uint32_t bound = 0, logical = 0;
	for (uint32_t i = 0; i < r.parser.NodeCount(); ++i)
	{
		if (r.parser.Node(i).mKind == QueryNodeKind::Compare) { bound += r.resolver.Binding(i).mBound ? 1u : 0u; }
		else { logical += r.resolver.Binding(i).mBound ? 1u : 0u; }
	}
	EXPECT_EQ(2u, bound);
	EXPECT_EQ(0u, logical);
}

// ---- Evaluator ----

TEST(QueryExhaustiveEval, StateBeforeAnyEvaluationIsEmpty)
{
	QueryEvaluator e;
	EXPECT_EQ(0u, e.CellCount());
	EXPECT_EQ(0u, e.WordCount());
	EXPECT_EQ(nullptr, e.Words());
	EXPECT_EQ(0u, e.Count());
	EXPECT_FALSE(e.Test(0));
	EXPECT_EQ('\0', e.Error().mMessage[0]);
}

TEST(QueryExhaustiveEval, TestBeyondCellCountIsFalseAndWordCountRoundsUp)
{
	SmallEval s(65, 1, std::vector<float>(65, 1.0f));
	ASSERT_TRUE(s.f.Eval("x > 0"));
	EXPECT_EQ(65u, s.f.evaluator.CellCount());
	EXPECT_EQ(2u, s.f.evaluator.WordCount());
	EXPECT_TRUE(s.f.evaluator.Test(64));
	EXPECT_FALSE(s.f.evaluator.Test(65));	// tail bit is zero and out of range
	EXPECT_FALSE(s.f.evaluator.Test(127));
	EXPECT_FALSE(s.f.evaluator.Test(0xFFFFFFFFu));
	EXPECT_EQ(65u, s.f.evaluator.Count());
}

TEST(QueryExhaustiveEval, EvaluatingAnUnparsedParserFailsWithoutResult)
{
	SmallEval s(4, 1, { 1, 2, 3, 4 });
	QueryParser empty;	// never parsed: RootIndex is kNone
	EXPECT_FALSE(s.f.evaluator.Evaluate(empty, s.f.resolver, s.f.Reader(), 4));
	EXPECT_STREQ("nothing to evaluate", s.f.evaluator.Error().mMessage);
	EXPECT_EQ(nullptr, s.f.evaluator.Words());
}

TEST(QueryExhaustiveEval, ResultIsDeterministicAcrossFreshAndReusedEvaluators)
{
	std::vector<float> v;
	for (uint32_t i = 0; i < 130; ++i) { v.push_back(i % 13 == 0 ? kNaN : static_cast<float>((i * 37u) % 101u) / 100.0f); }
	SmallEval s(130, 1, v);
	const char* q = "not (x < 0.3 or x >= 0.8) and x != 0.5";
	ASSERT_TRUE(s.f.Eval(q));
	const Cells first = s.f.Result();
	for (int i = 0; i < 3; ++i) { ASSERT_TRUE(s.f.Eval(q)); EXPECT_EQ(first, s.f.Result()); }

	QueryEvaluator fresh;
	ASSERT_TRUE(fresh.Evaluate(s.f.parser, s.f.resolver, s.f.Reader(), 130));
	ASSERT_EQ(s.f.evaluator.WordCount(), fresh.WordCount());
	for (uint32_t w = 0; w < fresh.WordCount(); ++w) { EXPECT_EQ(s.f.evaluator.Words()[w], fresh.Words()[w]) << w; }

	// operand order of and/or does not change the set
	ASSERT_TRUE(s.f.Eval("x != 0.5 and not (x >= 0.8 or x < 0.3)"));
	EXPECT_EQ(first, s.f.Result());
}

TEST(QueryExhaustiveEval, DeMorganHoldsIncludingOnNaNCells)
{
	std::vector<float> v;
	for (uint32_t i = 0; i < 100; ++i) { v.push_back(i % 7 == 0 ? kNaN : static_cast<float>(i) / 100.0f); }
	QueryEvalFixture f(10, 10);
	f.PublishSchema({ "x", "y" });
	std::vector<float> w = v;
	std::reverse(w.begin(), w.end());
	ASSERT_TRUE(f.AddValues("x", v));
	ASSERT_TRUE(f.AddValues("y", w));
	ASSERT_TRUE(f.Eval("not (x < 0.5 and y > 0.2)"));
	const Cells lhs = f.Result();
	ASSERT_TRUE(f.Eval("not x < 0.5 or not y > 0.2"));
	EXPECT_EQ(lhs, f.Result());
	ASSERT_TRUE(f.Eval("not (x < 0.5 or y > 0.2)"));
	const Cells lhs2 = f.Result();
	ASSERT_TRUE(f.Eval("not x < 0.5 and not y > 0.2"));
	EXPECT_EQ(lhs2, f.Result());
	// NaN cells (index % 7 == 0) are in the complement of every comparison
	ASSERT_TRUE(f.Eval("not x < 0.5"));
	EXPECT_TRUE(f.evaluator.Test(0));
}

TEST(QueryExhaustiveEval, FloatNarrowingIsDocumentedBehaviourForTinyThresholds)
{
	// The threshold is narrowed to float before comparing (QueryEvaluator.h). A literal far below the smallest float denormal
	// therefore equals 0 for == and != purposes. Pinned so a change is deliberate.
	SmallEval s(3, 1, { 0.0f, 1.0e-45f, 1.0f });
	const std::string tiny = "0." + std::string(60, '0') + "1";
	ASSERT_TRUE(s.f.Eval(("x == " + tiny).c_str()));
	EXPECT_EQ((Cells{ 0 }), s.f.Result());
	ASSERT_TRUE(s.f.Eval(("x > " + tiny).c_str()));
	EXPECT_EQ((Cells{ 1, 2 }), s.f.Result());
}

TEST(QueryExhaustiveEval, LayerReadWithMoreValuesThanTheGridFailsAtTheComparisonName)
{
	SmallEval s(4, 1, { 1, 2, 3, 4 });
	ASSERT_TRUE(s.f.q.Run("x > 1", s.f.parser, s.f.resolver));
	EXPECT_FALSE(s.f.evaluator.Evaluate(s.f.parser, s.f.resolver, s.f.Reader(), 3));	// caller disagrees with the layer
	EXPECT_STREQ("no data for layer", s.f.evaluator.Error().mMessage);
	EXPECT_EQ(0u, s.f.evaluator.Error().mColumn);
	EXPECT_EQ(1u, s.f.evaluator.Error().mLength);
	EXPECT_EQ(nullptr, s.f.evaluator.Words());
	EXPECT_EQ(0u, s.f.evaluator.Count());
}

// ---- Randomized differential test against a per-cell reference interpreter ----

namespace
{
	constexpr uint32_t kFuzzCells = 81;	// 9x9: not a multiple of 64

	struct FuzzData
	{
		std::vector<float> a, b, c;
	};

	// Generates a random grammar-conformant query and, in lockstep, its per-cell truth table.
	struct FuzzGen
	{
		Lcg rng;
		const FuzzData& d;
		FuzzGen(uint32_t seed, const FuzzData& data) : rng{ seed }, d(data) {}

		typedef std::vector<bool> Bits;

		static bool Cmp(float v, uint32_t op, float t)
		{
			if (v != v) { return false; }
			switch (op)
			{
			case 0: return v < t;
			case 1: return v <= t;
			case 2: return v > t;
			case 3: return v >= t;
			case 4: return v == t;
			default: return v != t;
			}
		}

		std::string Compare(Bits& out)
		{
			static const char* const kOps[] = { "<", "<=", ">", ">=", "==", "!=" };
			static const float kTs[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
			out.assign(kFuzzCells, false);
			if (rng.Below(3) == 0)
			{
				static const char* const kIds[] = { "Grass", "Water", "Rock", "Sand" };
				const uint32_t id = rng.Below(4);
				const bool ne = rng.Below(2) == 0;
				for (uint32_t i = 0; i < kFuzzCells; ++i)
				{
					const float v = d.c[i];
					const bool eq = (v == v) && static_cast<uint32_t>(v) == id;
					out[i] = (v == v) && (ne ? !eq : eq);
				}
				return std::string("c ") + (ne ? "!=" : (rng.Below(2) ? "=" : "==")) + " " + kIds[id];
			}
			const bool useA = rng.Below(2) == 0;
			const uint32_t op = rng.Below(6);
			const float t = kTs[rng.Below(5)];
			const std::vector<float>& v = useA ? d.a : d.b;
			for (uint32_t i = 0; i < kFuzzCells; ++i) { out[i] = Cmp(v[i], op, t); }
			char buf[64];
			snprintf(buf, sizeof(buf), "%s %s %.2f", useA ? "a" : "b", kOps[op], t);
			return buf;
		}

		std::string Primary(uint32_t parens, Bits& out)
		{
			if (parens < 3 && rng.Below(4) == 0)
			{
				return "(" + Or(parens + 1, out) + ")";
			}
			return Compare(out);
		}

		std::string Not(uint32_t parens, uint32_t nots, Bits& out)
		{
			if (nots < 3 && rng.Below(3) == 0)
			{
				Bits inner;
				const std::string s = Not(parens, nots + 1, inner);
				out.assign(kFuzzCells, false);
				for (uint32_t i = 0; i < kFuzzCells; ++i) { out[i] = !inner[i]; }
				return "not " + s;
			}
			return Primary(parens, out);
		}

		std::string And(uint32_t parens, Bits& out)
		{
			const uint32_t n = 1 + rng.Below(3);
			std::string s = Not(parens, 0, out);
			for (uint32_t k = 1; k < n; ++k)
			{
				Bits r;
				s += " and " + Not(parens, 0, r);
				for (uint32_t i = 0; i < kFuzzCells; ++i) { out[i] = out[i] && r[i]; }
			}
			return s;
		}

		std::string Or(uint32_t parens, Bits& out)
		{
			const uint32_t n = 1 + rng.Below(3);
			std::string s = And(parens, out);
			for (uint32_t k = 1; k < n; ++k)
			{
				Bits r;
				s += " or " + And(parens, r);
				for (uint32_t i = 0; i < kFuzzCells; ++i) { out[i] = out[i] || r[i]; }
			}
			return s;
		}
	};
}

TEST(QueryExhaustiveFuzz, RandomQueriesMatchAPerCellReferenceInterpreter)
{
	FuzzData d;
	Lcg dataRng{ 12345u };
	const float kVals[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.3f };
	for (uint32_t i = 0; i < kFuzzCells; ++i)
	{
		d.a.push_back(dataRng.Below(9) == 0 ? kNaN : kVals[dataRng.Below(6)]);
		d.b.push_back(dataRng.Below(9) == 0 ? kNaN : kVals[dataRng.Below(6)]);
		d.c.push_back(dataRng.Below(9) == 0 ? kNaN : static_cast<float>(dataRng.Below(4)));
	}
	QueryEvalFixture f(9, 9);
	f.PublishSchema({ "a", "b" }, { "c" });
	ASSERT_TRUE(f.AddValues("a", d.a));
	ASSERT_TRUE(f.AddValues("b", d.b));
	ASSERT_TRUE(f.AddValues("c", d.c, CellValueKind::Category));

	uint32_t ran = 0;
	uint32_t nonTrivial = 0;
	for (uint32_t seed = 1; seed <= 600; ++seed)
	{
		FuzzGen gen(seed, d);
		FuzzGen::Bits expected;
		const std::string text = gen.Or(0, expected);
		if (text.size() > QueryLexer::kMaxInput) { continue; }
		ASSERT_TRUE(f.Eval(text.c_str())) << "seed " << seed << ": " << text << " : " << f.Msg();
		uint32_t expectedCount = 0;
		for (uint32_t i = 0; i < kFuzzCells; ++i)
		{
			ASSERT_EQ(static_cast<bool>(expected[i]), f.evaluator.Test(i)) << "seed " << seed << " cell " << i << ": " << text;
			expectedCount += expected[i] ? 1u : 0u;
		}
		ASSERT_EQ(expectedCount, f.evaluator.Count()) << "seed " << seed << ": " << text;
		ASSERT_EQ(0u, f.evaluator.Words()[1] >> (kFuzzCells - 64)) << "tail bits dirty, seed " << seed;
		++ran;
		if (expectedCount != 0 && expectedCount != kFuzzCells) { ++nonTrivial; }
	}
	EXPECT_GE(ran, 500u);
	EXPECT_GE(nonTrivial, 200u);	// the corpus must exercise real selections, not only empty/full sets
}
