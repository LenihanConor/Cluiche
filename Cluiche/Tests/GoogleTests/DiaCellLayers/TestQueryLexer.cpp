#include <gtest/gtest.h>
#include <DiaCellLayers/QueryLexer.h>

#include <string>

using namespace Dia::CellLayers;

namespace
{
	std::string Text(const std::string& src, const QueryToken& t)
	{
		return src.substr(t.mColumn, t.mLength);
	}
}

TEST(QueryLexerTest, EmptyInputYieldsOnlyEnd)
{
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize(""));
	ASSERT_EQ(lx.TokenCount(), 1u);
	EXPECT_EQ(lx.Token(0).mKind, QueryTokenKind::End);
	EXPECT_EQ(lx.Token(0).mColumn, 0u);
}

TEST(QueryLexerTest, WhitespaceOnlyYieldsEnd)
{
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize("  \t "));
	ASSERT_EQ(lx.TokenCount(), 1u);
	EXPECT_EQ(lx.Token(0).mKind, QueryTokenKind::End);
}

TEST(QueryLexerTest, AllOperatorKinds)
{
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize("< <= > >= == = !="));
	ASSERT_EQ(lx.TokenCount(), 8u);
	EXPECT_EQ(lx.Token(0).mKind, QueryTokenKind::Less);
	EXPECT_EQ(lx.Token(1).mKind, QueryTokenKind::LessEqual);
	EXPECT_EQ(lx.Token(2).mKind, QueryTokenKind::Greater);
	EXPECT_EQ(lx.Token(3).mKind, QueryTokenKind::GreaterEqual);
	EXPECT_EQ(lx.Token(4).mKind, QueryTokenKind::Equal);
	EXPECT_EQ(lx.Token(5).mKind, QueryTokenKind::Equal);
	EXPECT_EQ(lx.Token(6).mKind, QueryTokenKind::NotEqual);
	EXPECT_EQ(lx.Token(7).mKind, QueryTokenKind::End);
	EXPECT_EQ(lx.Token(1).mLength, 2u);
	EXPECT_EQ(lx.Token(4).mLength, 2u);
	EXPECT_EQ(lx.Token(5).mLength, 1u);
	EXPECT_EQ(lx.Token(6).mColumn, 15u);
}

TEST(QueryLexerTest, ParenthesesAndKeywords)
{
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize("not (a and b) or c"));
	ASSERT_EQ(lx.TokenCount(), 9u);
	EXPECT_EQ(lx.Token(0).mKind, QueryTokenKind::Not);
	EXPECT_EQ(lx.Token(1).mKind, QueryTokenKind::LParen);
	EXPECT_EQ(lx.Token(2).mKind, QueryTokenKind::Identifier);
	EXPECT_EQ(lx.Token(3).mKind, QueryTokenKind::And);
	EXPECT_EQ(lx.Token(4).mKind, QueryTokenKind::Identifier);
	EXPECT_EQ(lx.Token(5).mKind, QueryTokenKind::RParen);
	EXPECT_EQ(lx.Token(6).mKind, QueryTokenKind::Or);
	EXPECT_EQ(lx.Token(7).mKind, QueryTokenKind::Identifier);
	EXPECT_EQ(lx.Token(8).mKind, QueryTokenKind::End);
	EXPECT_EQ(lx.Token(3).mColumn, 7u);
	EXPECT_EQ(lx.Token(3).mLength, 3u);
}

TEST(QueryLexerTest, KeywordsAreCaseSensitive)
{
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize("AND Or NOT"));
	EXPECT_EQ(lx.Token(0).mKind, QueryTokenKind::Identifier);
	EXPECT_EQ(lx.Token(1).mKind, QueryTokenKind::Identifier);
	EXPECT_EQ(lx.Token(2).mKind, QueryTokenKind::Identifier);
}

TEST(QueryLexerTest, KeywordPrefixIsIdentifier)
{
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize("android order nothing"));
	EXPECT_EQ(lx.Token(0).mKind, QueryTokenKind::Identifier);
	EXPECT_EQ(lx.Token(1).mKind, QueryTokenKind::Identifier);
	EXPECT_EQ(lx.Token(2).mKind, QueryTokenKind::Identifier);
}

TEST(QueryLexerTest, IdentifiersWithDotsDigitsUnderscores)
{
	const std::string src = "x.y_z _a b2.c";
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize(src.c_str()));
	ASSERT_EQ(lx.TokenCount(), 4u);
	EXPECT_EQ(Text(src, lx.Token(0)), "x.y_z");
	EXPECT_EQ(Text(src, lx.Token(1)), "_a");
	EXPECT_EQ(Text(src, lx.Token(2)), "b2.c");
	EXPECT_EQ(lx.Token(2).mColumn, 9u);
}

TEST(QueryLexerTest, NumbersWithSignAndFraction)
{
	const std::string src = "12 -0.5 +3 4.25";
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize(src.c_str()));
	ASSERT_EQ(lx.TokenCount(), 5u);
	for (uint32_t i = 0; i < 4; ++i) { EXPECT_EQ(lx.Token(i).mKind, QueryTokenKind::Number); }
	EXPECT_EQ(Text(src, lx.Token(0)), "12");
	EXPECT_EQ(Text(src, lx.Token(1)), "-0.5");
	EXPECT_EQ(Text(src, lx.Token(2)), "+3");
	EXPECT_EQ(Text(src, lx.Token(3)), "4.25");
}

TEST(QueryLexerTest, NoSpacesBetweenTokens)
{
	const std::string src = "a<=3";
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize(src.c_str()));
	ASSERT_EQ(lx.TokenCount(), 4u);
	EXPECT_EQ(lx.Token(0).mKind, QueryTokenKind::Identifier);
	EXPECT_EQ(lx.Token(1).mKind, QueryTokenKind::LessEqual);
	EXPECT_EQ(lx.Token(1).mColumn, 1u);
	EXPECT_EQ(lx.Token(2).mKind, QueryTokenKind::Number);
	EXPECT_EQ(lx.Token(2).mColumn, 3u);
}

TEST(QueryLexerTest, NegativeNumberAfterOperator)
{
	const std::string src = "h>-0.5";
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize(src.c_str()));
	ASSERT_EQ(lx.TokenCount(), 4u);
	EXPECT_EQ(lx.Token(1).mKind, QueryTokenKind::Greater);
	EXPECT_EQ(Text(src, lx.Token(2)), "-0.5");
}

TEST(QueryLexerTest, BadNumbersReportColumnAndLength)
{
	QueryLexer lx;
	EXPECT_FALSE(lx.Tokenize("a < 1e5"));
	EXPECT_EQ(lx.Error().mColumn, 4u);
	EXPECT_EQ(lx.Error().mLength, 3u);
	EXPECT_EQ(lx.TokenCount(), 0u);

	EXPECT_FALSE(lx.Tokenize("a < 5%"));
	EXPECT_EQ(lx.Error().mColumn, 4u);
	EXPECT_EQ(lx.Error().mLength, 2u);

	EXPECT_FALSE(lx.Tokenize("a < 1."));
	EXPECT_EQ(lx.Error().mColumn, 4u);
	EXPECT_EQ(lx.Error().mLength, 2u);

	EXPECT_FALSE(lx.Tokenize("a < .5"));
	EXPECT_EQ(lx.Error().mColumn, 4u);
	EXPECT_EQ(lx.Error().mLength, 2u);

	EXPECT_FALSE(lx.Tokenize("a < -"));
	EXPECT_EQ(lx.Error().mColumn, 4u);
	EXPECT_EQ(lx.Error().mLength, 1u);
}

TEST(QueryLexerTest, UnknownCharacterIsError)
{
	QueryLexer lx;
	EXPECT_FALSE(lx.Tokenize("a $ b"));
	EXPECT_EQ(lx.Error().mColumn, 2u);
	EXPECT_EQ(lx.Error().mLength, 1u);
	EXPECT_NE(lx.Error().mMessage[0], '\0');
}

TEST(QueryLexerTest, LoneBangIsError)
{
	QueryLexer lx;
	EXPECT_FALSE(lx.Tokenize("a ! b"));
	EXPECT_EQ(lx.Error().mColumn, 2u);
	EXPECT_EQ(lx.Error().mLength, 1u);
}

TEST(QueryLexerTest, Exactly512CharsIsAccepted)
{
	const std::string src(512, 'a');
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize(src.c_str()));
	ASSERT_EQ(lx.TokenCount(), 2u);
	EXPECT_EQ(lx.Token(0).mLength, 512u);
}

TEST(QueryLexerTest, MaxTokensFitsCapacity)
{
	std::string src;
	for (uint32_t i = 0; i < 256; ++i) { src += "()"; }
	ASSERT_EQ(src.size(), 512u);
	QueryLexer lx;
	ASSERT_TRUE(lx.Tokenize(src.c_str()));
	EXPECT_EQ(lx.TokenCount(), 513u);
}

TEST(QueryLexerTest, Over512CharsIsError)
{
	const std::string src(515, 'a');
	QueryLexer lx;
	EXPECT_FALSE(lx.Tokenize(src.c_str()));
	EXPECT_EQ(lx.Error().mColumn, 512u);
	EXPECT_EQ(lx.Error().mLength, 3u);
}

TEST(QueryLexerTest, ReusableAfterError)
{
	QueryLexer lx;
	EXPECT_FALSE(lx.Tokenize("$"));
	ASSERT_TRUE(lx.Tokenize("a"));
	EXPECT_EQ(lx.TokenCount(), 2u);
	EXPECT_EQ(lx.Error().mMessage[0], '\0');
}
