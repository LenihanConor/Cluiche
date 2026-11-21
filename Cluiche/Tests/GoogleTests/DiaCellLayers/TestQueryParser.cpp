#include <gtest/gtest.h>
#include <DiaCellLayers/QueryParser.h>

#include <string.h>
#include <string>

using namespace Dia::CellLayers;

namespace
{
	const char* OpText(QueryCompareOp op)
	{
		switch (op)
		{
		case QueryCompareOp::Lt: return "<";
		case QueryCompareOp::Le: return "<=";
		case QueryCompareOp::Gt: return ">";
		case QueryCompareOp::Ge: return ">=";
		case QueryCompareOp::Eq: return "=";
		case QueryCompareOp::Ne: return "!=";
		}
		return "?";
	}

	// Renders the tree as a prefix string: And(x,y), Or(x,y), Not(x), and name<op>rhs for comparisons.
	std::string Dump(const QueryParser& p, const std::string& src, uint32_t i)
	{
		const QueryNode& n = p.Node(i);
		switch (n.mKind)
		{
		case QueryNodeKind::And: return "And(" + Dump(p, src, n.mLeft) + "," + Dump(p, src, n.mRight) + ")";
		case QueryNodeKind::Or: return "Or(" + Dump(p, src, n.mLeft) + "," + Dump(p, src, n.mRight) + ")";
		case QueryNodeKind::Not: return "Not(" + Dump(p, src, n.mLeft) + ")";
		case QueryNodeKind::Compare:
			return src.substr(n.mName.mColumn, n.mName.mLength) + OpText(n.mOp) + src.substr(n.mRhs.mColumn, n.mRhs.mLength);
		}
		return "?";
	}

	std::string Tree(const std::string& src)
	{
		QueryParser p;
		if (!p.Parse(src.c_str())) { return std::string("ERROR: ") + p.Error().mMessage; }
		return Dump(p, src, p.RootIndex());
	}

	QueryError ErrorOf(const std::string& src)
	{
		QueryParser p;
		EXPECT_FALSE(p.Parse(src.c_str())) << src;
		return p.Error();
	}

	bool Has(const QueryError& e, const char* what)
	{
		return std::string(e.mMessage).find(what) != std::string::npos;
	}

	std::string NestedParens(uint32_t depth)
	{
		return std::string(depth, '(') + "a<1" + std::string(depth, ')');
	}
}

TEST(QueryParserTest, SingleNumericComparison)
{
	QueryParser p;
	ASSERT_TRUE(p.Parse("temperature < 12.5"));
	ASSERT_EQ(p.NodeCount(), 1u);
	const QueryNode& n = p.Node(p.RootIndex());
	EXPECT_EQ(n.mKind, QueryNodeKind::Compare);
	EXPECT_EQ(n.mOp, QueryCompareOp::Lt);
	EXPECT_EQ(n.mRhsKind, QueryRhsKind::Number);
	EXPECT_DOUBLE_EQ(n.mValue, 12.5);
}

TEST(QueryParserTest, ComparisonPositions)
{
	QueryParser p;
	ASSERT_TRUE(p.Parse("  temp  >=  -3"));
	const QueryNode& n = p.Node(p.RootIndex());
	EXPECT_EQ(n.mOp, QueryCompareOp::Ge);
	EXPECT_EQ(n.mName.mColumn, 2u);
	EXPECT_EQ(n.mName.mLength, 4u);
	EXPECT_EQ(n.mOpSpan.mColumn, 8u);
	EXPECT_EQ(n.mOpSpan.mLength, 2u);
	EXPECT_EQ(n.mRhs.mColumn, 12u);
	EXPECT_EQ(n.mRhs.mLength, 2u);
	EXPECT_DOUBLE_EQ(n.mValue, -3.0);
}

TEST(QueryParserTest, AllOperators)
{
	EXPECT_EQ(Tree("a<1"), "a<1");
	EXPECT_EQ(Tree("a<=1"), "a<=1");
	EXPECT_EQ(Tree("a>1"), "a>1");
	EXPECT_EQ(Tree("a>=1"), "a>=1");
	EXPECT_EQ(Tree("a!=1"), "a!=1");
	QueryParser p;
	ASSERT_TRUE(p.Parse("a<=1"));
	EXPECT_EQ(p.Node(p.RootIndex()).mOp, QueryCompareOp::Le);
	ASSERT_TRUE(p.Parse("a>1"));
	EXPECT_EQ(p.Node(p.RootIndex()).mOp, QueryCompareOp::Gt);
	ASSERT_TRUE(p.Parse("a!=1"));
	EXPECT_EQ(p.Node(p.RootIndex()).mOp, QueryCompareOp::Ne);
}

TEST(QueryParserTest, EqualAndDoubleEqualAreEquivalent)
{
	QueryParser a, b;
	ASSERT_TRUE(a.Parse("x = 4"));
	ASSERT_TRUE(b.Parse("x == 4"));
	EXPECT_EQ(a.Node(a.RootIndex()).mOp, QueryCompareOp::Eq);
	EXPECT_EQ(b.Node(b.RootIndex()).mOp, QueryCompareOp::Eq);
	EXPECT_EQ(b.Node(b.RootIndex()).mOpSpan.mLength, 2u);
}

TEST(QueryParserTest, EnumIdRightHandSide)
{
	QueryParser p;
	std::string src = "biome == forest";
	ASSERT_TRUE(p.Parse(src.c_str()));
	const QueryNode& n = p.Node(p.RootIndex());
	EXPECT_EQ(n.mRhsKind, QueryRhsKind::EnumId);
	EXPECT_EQ(src.substr(n.mRhs.mColumn, n.mRhs.mLength), "forest");
	ASSERT_TRUE(p.Parse("biome != forest"));
	EXPECT_EQ(p.Node(p.RootIndex()).mOp, QueryCompareOp::Ne);
	EXPECT_EQ(p.Node(p.RootIndex()).mRhsKind, QueryRhsKind::EnumId);
	ASSERT_TRUE(p.Parse("biome = forest"));
}

TEST(QueryParserTest, DottedIdentifierName)
{
	EXPECT_EQ(Tree("terrain.height > 3"), "terrain.height>3");
}

TEST(QueryParserTest, AndBindsTighterThanOr)
{
	EXPECT_EQ(Tree("a<1 or b<2 and c<3"), "Or(a<1,And(b<2,c<3))");
	EXPECT_EQ(Tree("a<1 and b<2 or c<3"), "Or(And(a<1,b<2),c<3)");
}

TEST(QueryParserTest, NotBindsTighterThanAnd)
{
	EXPECT_EQ(Tree("not a<1 and b<2"), "And(Not(a<1),b<2)");
	EXPECT_EQ(Tree("a<1 and not b<2"), "And(a<1,Not(b<2))");
}

TEST(QueryParserTest, ParenthesesOverridePrecedence)
{
	EXPECT_EQ(Tree("(a<1 or b<2) and c<3"), "And(Or(a<1,b<2),c<3)");
	EXPECT_EQ(Tree("not (a<1 and b<2)"), "Not(And(a<1,b<2))");
	EXPECT_EQ(Tree("((a<1))"), "a<1");
}

TEST(QueryParserTest, AndOrAreLeftAssociative)
{
	EXPECT_EQ(Tree("a<1 and b<2 and c<3"), "And(And(a<1,b<2),c<3)");
	EXPECT_EQ(Tree("a<1 or b<2 or c<3"), "Or(Or(a<1,b<2),c<3)");
}

TEST(QueryParserTest, StackedNot)
{
	EXPECT_EQ(Tree("not not x<1"), "Not(Not(x<1))");
}

TEST(QueryParserTest, ReparseResetsState)
{
	QueryParser p;
	ASSERT_TRUE(p.Parse("a<1 and b<2"));
	EXPECT_EQ(p.NodeCount(), 3u);
	ASSERT_TRUE(p.Parse("c<3"));
	EXPECT_EQ(p.NodeCount(), 1u);
	EXPECT_FALSE(p.Parse("c <"));
	EXPECT_EQ(p.NodeCount(), 0u);
}

// ---- errors ----

TEST(QueryParserTest, EmptyQueryIsMissingOperand)
{
	QueryError e = ErrorOf("");
	EXPECT_TRUE(Has(e, "missing operand")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 0u);
	EXPECT_EQ(e.mLength, 0u);
	e = ErrorOf("   ");
	EXPECT_TRUE(Has(e, "missing operand")) << e.mMessage;
}

TEST(QueryParserTest, MissingRhsAfterOperator)
{
	QueryError e = ErrorOf("a <");
	EXPECT_TRUE(Has(e, "missing operand")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 3u);
	EXPECT_EQ(e.mLength, 0u);
}

TEST(QueryParserTest, MissingOperandAfterAnd)
{
	QueryError e = ErrorOf("a < 1 and");
	EXPECT_TRUE(Has(e, "missing operand")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 9u);
	EXPECT_EQ(e.mLength, 0u);
}

TEST(QueryParserTest, MissingOperandBeforeAnd)
{
	QueryError e = ErrorOf("and a<1");
	EXPECT_TRUE(Has(e, "missing operand")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 0u);
	EXPECT_EQ(e.mLength, 3u);
}

TEST(QueryParserTest, MissingOperandAfterNot)
{
	QueryError e = ErrorOf("not");
	EXPECT_TRUE(Has(e, "missing operand")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 3u);
}

TEST(QueryParserTest, TrailingTokenIsUnexpected)
{
	QueryError e = ErrorOf("a < 1 b");
	EXPECT_TRUE(Has(e, "unexpected token")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 6u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, LeadingOperatorIsUnexpected)
{
	QueryError e = ErrorOf("< 3");
	EXPECT_TRUE(Has(e, "unexpected token")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 0u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, NameFollowedByNameIsMissingOperator)
{
	QueryError e = ErrorOf("a b");
	EXPECT_TRUE(Has(e, "missing operator")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 2u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, NameFollowedByNumberIsMissingOperator)
{
	QueryError e = ErrorOf("a 3");
	EXPECT_TRUE(Has(e, "missing operator")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 2u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, NameAloneIsMissingOperator)
{
	QueryError e = ErrorOf("a");
	EXPECT_TRUE(Has(e, "missing operator")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 1u);
	EXPECT_EQ(e.mLength, 0u);
}

TEST(QueryParserTest, OrderingOperatorWithIdentifierRhsExpectsNumber)
{
	const char* ops[] = { "<", "<=", ">", ">=" };
	for (const char* op : ops)
	{
		std::string src = std::string("a ") + op + " forest";
		QueryError e = ErrorOf(src);
		EXPECT_TRUE(Has(e, "expected a number")) << e.mMessage;
		EXPECT_EQ(e.mColumn, 3u + static_cast<uint32_t>(strlen(op))) << src;
		EXPECT_EQ(e.mLength, 6u);
	}
}

TEST(QueryParserTest, UnclosedParenPointsAtOpenParen)
{
	QueryError e = ErrorOf("(a<1");
	EXPECT_TRUE(Has(e, "unclosed")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 0u);
	EXPECT_EQ(e.mLength, 1u);
	e = ErrorOf("a<1 and (b<2 or c<3");
	EXPECT_TRUE(Has(e, "unclosed")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 8u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, StrayCloseParenPointsAtIt)
{
	QueryError e = ErrorOf("a<1)");
	EXPECT_TRUE(Has(e, "unmatched")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 3u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, EmptyParensMissingOperand)
{
	QueryError e = ErrorOf("()");
	EXPECT_TRUE(Has(e, "missing operand")) << e.mMessage;
	EXPECT_EQ(e.mColumn, 1u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, LexerErrorsPropagateUnchanged)
{
	QueryLexer lx;
	ASSERT_FALSE(lx.Tokenize("a < $"));
	QueryError e = ErrorOf("a < $");
	EXPECT_STREQ(e.mMessage, lx.Error().mMessage);
	EXPECT_EQ(e.mColumn, lx.Error().mColumn);
	EXPECT_EQ(e.mLength, lx.Error().mLength);

	ASSERT_FALSE(lx.Tokenize("a < 1."));
	e = ErrorOf("a < 1.");
	EXPECT_STREQ(e.mMessage, lx.Error().mMessage);
	EXPECT_EQ(e.mColumn, lx.Error().mColumn);
}

TEST(QueryParserTest, DepthSixteenOkSeventeenError)
{
	QueryParser p;
	EXPECT_TRUE(p.Parse(NestedParens(16).c_str())) << p.Error().mMessage;
	QueryError e = ErrorOf(NestedParens(17));
	EXPECT_TRUE(Has(e, "deep")) << e.mMessage;
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, StackedNotCountsTowardDepth)
{
	QueryParser p;
	std::string ok;
	for (int i = 0; i < 16; ++i) { ok += "not "; }
	ok += "a<1";
	EXPECT_TRUE(p.Parse(ok.c_str())) << p.Error().mMessage;
	QueryError e = ErrorOf("not " + ok);
	EXPECT_TRUE(Has(e, "deep")) << e.mMessage;
}

TEST(QueryParserTest, MixedParenAndNotDepth)
{
	std::string src;
	for (int i = 0; i < 9; ++i) { src += "(not "; }
	src += "a<1" + std::string(9, ')');
	QueryError e = ErrorOf(src);	// 18 levels
	EXPECT_TRUE(Has(e, "deep")) << e.mMessage;
}

TEST(QueryParserTest, MaxLengthOkOneOverError)
{
	std::string ok = "a<1";
	while (ok.size() < 512) { ok += " "; }
	QueryParser p;
	EXPECT_TRUE(p.Parse(ok.c_str())) << p.Error().mMessage;
	QueryError e = ErrorOf(ok + " ");
	EXPECT_EQ(e.mColumn, 512u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserTest, LongChainFitsNodePool)
{
	std::string src = "a<1";
	while (src.size() + 8 <= 512) { src += " and a<1"; }
	QueryParser p;
	ASSERT_TRUE(p.Parse(src.c_str())) << p.Error().mMessage;
	EXPECT_LE(p.NodeCount(), QueryParser::kMaxNodes);
}

// ---- Task 10a exhaustive pass ----

TEST(QueryParserEdge, TooDeepParenErrorPointsAtTheSeventeenthOpenParen)
{
	const QueryError e = ErrorOf(NestedParens(17));
	EXPECT_EQ(e.mColumn, 16u);
	EXPECT_EQ(e.mLength, 1u);
}

TEST(QueryParserEdge, TooDeepNotErrorPointsAtTheSeventeenthNotKeyword)
{
	std::string src;
	for (int i = 0; i < 17; ++i) { src += "not "; }
	src += "a<1";
	const QueryError e = ErrorOf(src);
	EXPECT_EQ(e.mColumn, 64u);	// 16 * len("not ")
	EXPECT_EQ(e.mLength, 3u);
}

TEST(QueryParserEdge, SiblingParenGroupsDoNotAccumulateDepth)
{
	std::string src;
	for (int i = 0; i < 40; ++i) { src += (i ? " and (a<1)" : "(a<1)"); }
	QueryParser p;
	EXPECT_TRUE(p.Parse(src.c_str())) << p.Error().mMessage;	// depth is nesting, not count
}

TEST(QueryParserEdge, NumberValuesAreParsedWithSignAndFraction)
{
	const struct { const char* text; double value; } cases[] = {
		{ "a<-0.5", -0.5 }, { "a<+3", 3.0 }, { "a<007", 7.0 }, { "a<0.125", 0.125 }, { "a<-0", 0.0 },
	};
	for (const auto& c : cases)
	{
		QueryParser p;
		ASSERT_TRUE(p.Parse(c.text)) << c.text << p.Error().mMessage;
		EXPECT_DOUBLE_EQ(p.Node(p.RootIndex()).mValue, c.value) << c.text;
		EXPECT_EQ(p.Node(p.RootIndex()).mRhsKind, QueryRhsKind::Number);
	}
}

TEST(QueryParserEdge, VeryLongDigitStringStillParsesAsANumber)
{
	const std::string src = "a<" + std::string(300, '9');
	QueryParser p;
	ASSERT_TRUE(p.Parse(src.c_str())) << p.Error().mMessage;
	EXPECT_GT(p.Node(p.RootIndex()).mValue, 1e100);
}

TEST(QueryParserEdge, KeywordsCannotStandInForNamesOrValues)
{
	EXPECT_TRUE(Has(ErrorOf("and < 1"), "missing operand"));
	EXPECT_TRUE(Has(ErrorOf("not < 1"), "unexpected token"));
	const QueryError rhs = ErrorOf("a == not");
	EXPECT_EQ(rhs.mColumn, 5u);
	EXPECT_EQ(rhs.mLength, 3u);
	EXPECT_TRUE(Has(ErrorOf("a < and b < 1"), "missing operand"));
}

TEST(QueryParserEdge, DoubledOperatorAndParenAsValueAreUnexpected)
{
	QueryError e = ErrorOf("a = = 1");
	EXPECT_EQ(e.mColumn, 4u);
	EXPECT_EQ(e.mLength, 1u);
	e = ErrorOf("a < (1)");
	EXPECT_EQ(e.mColumn, 4u);
	EXPECT_TRUE(Has(e, "expected a number"));
	e = ErrorOf("a == (b)");
	EXPECT_EQ(e.mColumn, 5u);
	EXPECT_TRUE(Has(e, "number or identifier"));
}

TEST(QueryParserEdge, ErrorsLeaveNoTreeAndParserIsReusable)
{
	QueryParser p;
	EXPECT_FALSE(p.Parse("a < 1 and"));
	EXPECT_EQ(p.NodeCount(), 0u);
	EXPECT_EQ(p.RootIndex(), QueryNode::kNone);
	ASSERT_TRUE(p.Parse("a < 1"));
	EXPECT_EQ(p.NodeCount(), 1u);
	EXPECT_EQ(p.Error().mMessage[0], '\0');
}

TEST(QueryParserEdge, TrailingCloseParenAfterGroupIsUnmatched)
{
	QueryError e = ErrorOf("(a<1))");
	EXPECT_EQ(e.mColumn, 5u);
	EXPECT_TRUE(Has(e, "unmatched"));
	e = ErrorOf("a<1)");
	EXPECT_EQ(e.mColumn, 3u);
	EXPECT_TRUE(Has(e, "unmatched"));
}

TEST(QueryParserEdge, UnclosedInnerParenPointsAtInnermostUnclosedOpen)
{
	const QueryError e = ErrorOf("(a<1 and (b<2)");
	EXPECT_EQ(e.mColumn, 0u);	// the outer '(' never closes
	EXPECT_TRUE(Has(e, "unclosed"));
}

TEST(QueryParserEdge, SixteenNestedGroupsRightLeaningParse)
{
	std::string src;
	for (int i = 0; i < 16; ++i) { src += "a<1 or ("; }
	src += "a<1" + std::string(16, ')');
	QueryParser p;
	EXPECT_TRUE(p.Parse(src.c_str())) << p.Error().mMessage;
	EXPECT_LE(src.size(), 512u);
}
