#include <gtest/gtest.h>
#include <DiaCellLayers/QueryResolver.h>
#include <DiaCellLayers/Testing/QueryTestSupport.h>

#include <string.h>
#include <string>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;

namespace
{
	struct QueryResolverTest : ::testing::Test
	{
		QueryFixture	q;
		QueryParser		parser;
		QueryResolver	resolver;

		// Grid A: "temperature" (schema cells/temp), "terrain" category (cells/terrain), "humid" (cells/humidity).
		// Grid B: "other" (cells/elev).
		void Populate()
		{
			q.fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			q.fx.Add(UploaderSchemaFixture::FloatEntry("humidity", 0, 1));
			q.fx.Add(UploaderSchemaFixture::FloatEntry("elev", 0, 100));
			q.fx.Add(QueryFixture::Category("terrain", { "grass", "water", "rock", "Sand" }));
			q.Publish();
			ASSERT_TRUE(q.AddLayer("temperature", &q.gridA, "cells/temp"));
			ASSERT_TRUE(q.AddLayer("terrain", &q.gridA, "cells/terrain", CellValueKind::Category));
			ASSERT_TRUE(q.AddLayer("humid", &q.gridA, "cells/humidity"));
			ASSERT_TRUE(q.AddLayer("other", &q.gridB, "cells/elev"));
		}

		bool Run(const char* text) { return q.Run(text, parser, resolver); }
		std::string Msg() const { return resolver.Error().mMessage; }
		bool MsgHas(const char* s) const { return Msg().find(s) != std::string::npos; }
	};
}

TEST_F(QueryResolverTest, RegisteredIdResolves)
{
	Populate();
	ASSERT_TRUE(Run("temperature > 5.5")) << Msg();
	const QueryBinding& b = resolver.Binding(parser.RootIndex());
	EXPECT_TRUE(b.mBound);
	EXPECT_EQ(Dia::Core::StringCRC("temperature"), b.mLayerId);
	EXPECT_EQ(CellValueKind::Float, b.mKind);
	EXPECT_EQ(QueryCompareOp::Gt, b.mOp);
	EXPECT_DOUBLE_EQ(5.5, b.mThreshold);
	EXPECT_EQ(q.registry.Find(b.mLayerId), b.mLayer);
}

TEST_F(QueryResolverTest, UniqueShortKeyResolves)
{
	Populate();
	ASSERT_TRUE(Run("temp <= 3")) << Msg();
	const QueryBinding& b = resolver.Binding(parser.RootIndex());
	EXPECT_EQ(Dia::Core::StringCRC("temperature"), b.mLayerId);
	EXPECT_EQ(QueryCompareOp::Le, b.mOp);
}

TEST_F(QueryResolverTest, BindsEveryCompareNodeInCompound)
{
	Populate();
	ASSERT_TRUE(Run("temp > 1 and not humidity < 0.5")) << Msg();
	uint32_t bound = 0;
	for (uint32_t i = 0; i < parser.NodeCount(); ++i) { bound += resolver.Binding(i).mBound ? 1 : 0; }
	EXPECT_EQ(2u, bound);
}

TEST_F(QueryResolverTest, AmbiguousShortKeyListsBothCandidates)
{
	q.fx.Add(UploaderSchemaFixture::FloatEntry("a/x", 0, 1));
	q.fx.Add(UploaderSchemaFixture::FloatEntry("b/x", 0, 1));
	q.Publish();
	ASSERT_TRUE(q.AddLayer("la", &q.gridA, "cells/a/x"));
	ASSERT_TRUE(q.AddLayer("lb", &q.gridA, "cells/b/x"));
	EXPECT_FALSE(Run("x > 1"));
	EXPECT_TRUE(MsgHas("ambiguous layer name")) << Msg();
	EXPECT_TRUE(MsgHas("cells/a/x")) << Msg();
	EXPECT_TRUE(MsgHas("cells/b/x")) << Msg();
	EXPECT_EQ(0u, resolver.Error().mColumn);
	EXPECT_EQ(1u, resolver.Error().mLength);
}

TEST_F(QueryResolverTest, RegisteredIdWinsOverAmbiguousShortKey)
{
	q.fx.Add(UploaderSchemaFixture::FloatEntry("a/x", 0, 1));
	q.fx.Add(UploaderSchemaFixture::FloatEntry("b/x", 0, 1));
	q.Publish();
	ASSERT_TRUE(q.AddLayer("x", &q.gridA, "cells/a/x"));
	ASSERT_TRUE(q.AddLayer("lb", &q.gridA, "cells/b/x"));
	ASSERT_TRUE(Run("x > 1")) << Msg();
	EXPECT_EQ(Dia::Core::StringCRC("x"), resolver.Binding(parser.RootIndex()).mLayerId);
}

TEST_F(QueryResolverTest, UnknownWithCloseSuggestion)
{
	Populate();
	EXPECT_FALSE(Run("humidty > 1"));
	EXPECT_TRUE(MsgHas("unknown layer 'humidty'")) << Msg();
	EXPECT_STREQ("humidity", resolver.Error().mSuggestion);
	EXPECT_EQ(0u, resolver.Error().mColumn);
	EXPECT_EQ(7u, resolver.Error().mLength);
}

TEST_F(QueryResolverTest, UnknownWithoutCloseSuggestion)
{
	Populate();
	EXPECT_FALSE(Run("zzzzzz > 1"));
	EXPECT_TRUE(MsgHas("unknown layer 'zzzzzz'")) << Msg();
	EXPECT_FALSE(resolver.Error().HasSuggestion());
}

TEST_F(QueryResolverTest, UnknownErrorLocatesSecondComparison)
{
	Populate();
	EXPECT_FALSE(Run("temp > 1 and bogus < 2"));
	EXPECT_EQ(13u, resolver.Error().mColumn);
	EXPECT_EQ(5u, resolver.Error().mLength);
}

TEST_F(QueryResolverTest, WrongGridDoesNotResolve)
{
	Populate();
	EXPECT_FALSE(Run("other > 1"));
	EXPECT_TRUE(MsgHas("unknown layer 'other'")) << Msg();
	EXPECT_FALSE(Run("elev > 1"));
	// On grid B the same names resolve.
	QueryParser p;
	QueryResolver r;
	ASSERT_TRUE(q.Run("other > 1", p, r, &q.gridB)) << r.Error().mMessage;
}

TEST_F(QueryResolverTest, EnumExactResolvesToIndex)
{
	Populate();
	ASSERT_TRUE(Run("terrain = rock")) << Msg();
	const QueryBinding& b = resolver.Binding(parser.RootIndex());
	EXPECT_EQ(CellValueKind::Category, b.mKind);
	EXPECT_DOUBLE_EQ(2.0, b.mThreshold);
	EXPECT_EQ(QueryCompareOp::Eq, b.mOp);
	ASSERT_TRUE(Run("terrain != Sand")) << Msg();
	EXPECT_DOUBLE_EQ(3.0, resolver.Binding(parser.RootIndex()).mThreshold);
	EXPECT_EQ(QueryCompareOp::Ne, resolver.Binding(parser.RootIndex()).mOp);
}

TEST_F(QueryResolverTest, EnumWrongCaseIsErrorWithCorrectCaseSuggestion)
{
	Populate();
	EXPECT_FALSE(Run("terrain = Rock"));
	EXPECT_TRUE(MsgHas("unknown value 'Rock' for layer terrain")) << Msg();
	EXPECT_STREQ("rock", resolver.Error().mSuggestion);
	EXPECT_EQ(10u, resolver.Error().mColumn);
	EXPECT_EQ(4u, resolver.Error().mLength);
}

TEST_F(QueryResolverTest, EnumUnknownWithSuggestion)
{
	Populate();
	EXPECT_FALSE(Run("terrain = watr"));
	EXPECT_TRUE(MsgHas("unknown value 'watr'")) << Msg();
	EXPECT_STREQ("water", resolver.Error().mSuggestion);
}

TEST_F(QueryResolverTest, EnumUnknownWithoutSuggestion)
{
	Populate();
	EXPECT_FALSE(Run("terrain = qqqqqqq"));
	EXPECT_TRUE(MsgHas("unknown value 'qqqqqqq'")) << Msg();
	EXPECT_FALSE(resolver.Error().HasSuggestion());
}

TEST_F(QueryResolverTest, NumericOperatorOnCategoryIsError)
{
	Populate();
	EXPECT_FALSE(Run("terrain < 2"));
	EXPECT_TRUE(MsgHas("numeric comparison on category layer")) << Msg();
	EXPECT_EQ(8u, resolver.Error().mColumn);
	EXPECT_EQ(1u, resolver.Error().mLength);
}

TEST_F(QueryResolverTest, NumberRhsOnCategoryEqIsError)
{
	Populate();
	EXPECT_FALSE(Run("terrain = 2"));
	EXPECT_TRUE(MsgHas("expected a category id")) << Msg();
	EXPECT_EQ(10u, resolver.Error().mColumn);
	EXPECT_EQ(1u, resolver.Error().mLength);
}

TEST_F(QueryResolverTest, EnumRhsOnFloatLayerIsError)
{
	Populate();
	EXPECT_FALSE(Run("temp = hot"));
	EXPECT_TRUE(MsgHas("expected a number")) << Msg();
	EXPECT_EQ(7u, resolver.Error().mColumn);
	EXPECT_EQ(3u, resolver.Error().mLength);
}

TEST_F(QueryResolverTest, CategoryLayerWithoutSchemaIsError)
{
	Populate();
	ASSERT_TRUE(q.AddLayer("biome", &q.gridA, "cells/missing", CellValueKind::Category));
	EXPECT_FALSE(Run("biome = forest"));
	EXPECT_TRUE(MsgHas("no enum")) << Msg();
}

TEST_F(QueryResolverTest, NullSchemaStillResolvesByIdForFloat)
{
	Populate();
	q.service = nullptr;
	ASSERT_TRUE(Run("humid > 0.25")) << Msg();
	EXPECT_DOUBLE_EQ(0.25, resolver.Binding(parser.RootIndex()).mThreshold);
	EXPECT_FALSE(Run("humidity > 1"));
}
