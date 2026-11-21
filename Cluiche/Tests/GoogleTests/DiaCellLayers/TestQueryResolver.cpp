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

// ---- Task 10a exhaustive pass ----

TEST_F(QueryResolverTest, SuggestionRatioExactlyPointSixIsOffered)
{
	// Similarity = 1 - levenshtein / longest. "abcxx" vs "abcde": distance 2 of 5 = 0.6 exactly: "at least 0.6" suggests.
	q.fx.Add(UploaderSchemaFixture::FloatEntry("abcde", 0, 1));
	q.Publish();
	ASSERT_TRUE(q.AddLayer("lay", &q.gridA, "cells/abcde"));
	EXPECT_FALSE(Run("abcxx > 1"));
	EXPECT_STREQ("abcde", resolver.Error().mSuggestion);
}

TEST_F(QueryResolverTest, SuggestionRatioBelowPointSixIsNotOffered)
{
	q.fx.Add(UploaderSchemaFixture::FloatEntry("abcde", 0, 1));
	q.Publish();
	ASSERT_TRUE(q.AddLayer("lay", &q.gridA, "cells/abcde"));
	EXPECT_FALSE(Run("abxxx > 1"));	// distance 3 of 5 = 0.4
	EXPECT_FALSE(resolver.Error().HasSuggestion());
}

TEST_F(QueryResolverTest, EnumSuggestionRatioBoundaryMatchesLayerBoundary)
{
	q.fx.Add(QueryFixture::Category("terrain", { "abcde" }));
	q.Publish();
	ASSERT_TRUE(q.AddLayer("terrain", &q.gridA, "cells/terrain", CellValueKind::Category));
	EXPECT_FALSE(Run("terrain = abcxx"));
	EXPECT_STREQ("abcde", resolver.Error().mSuggestion);
	EXPECT_FALSE(Run("terrain = abxxx"));
	EXPECT_FALSE(resolver.Error().HasSuggestion());
}

TEST_F(QueryResolverTest, WrongCaseLayerNameSuggestsTheShortKey)
{
	Populate();
	EXPECT_FALSE(Run("TEMP > 1"));
	EXPECT_TRUE(MsgHas("unknown layer 'TEMP'")) << Msg();
	EXPECT_STREQ("temp", resolver.Error().mSuggestion);
}

TEST_F(QueryResolverTest, ThreeWayAmbiguityListsEveryCandidateInRegistrationOrder)
{
	for (const char* k : { "a/x", "b/x", "c/x" }) { q.fx.Add(UploaderSchemaFixture::FloatEntry(k, 0, 1)); }
	q.Publish();
	ASSERT_TRUE(q.AddLayer("l1", &q.gridA, "cells/a/x"));
	ASSERT_TRUE(q.AddLayer("l2", &q.gridA, "cells/b/x"));
	ASSERT_TRUE(q.AddLayer("l3", &q.gridA, "cells/c/x"));
	EXPECT_FALSE(Run("x > 1"));
	EXPECT_TRUE(MsgHas("cells/a/x, cells/b/x, cells/c/x")) << Msg();
}

TEST_F(QueryResolverTest, AmbiguityListIsTruncatedSafelyWithManyLongCandidates)
{
	std::vector<std::string> keys;
	for (int i = 0; i < 12; ++i) { keys.push_back("a_rather_long_group_name_number_" + std::to_string(i) + "/x"); }
	for (const std::string& k : keys) { q.fx.Add(UploaderSchemaFixture::FloatEntry(k.c_str(), 0, 1)); }
	q.Publish();
	for (int i = 0; i < 12; ++i) { ASSERT_TRUE(q.AddLayer(("l" + std::to_string(i)).c_str(), &q.gridA, ("cells/" + keys[i]).c_str())); }
	EXPECT_FALSE(Run("x > 1"));
	const std::string msg = Msg();
	EXPECT_LT(msg.size(), static_cast<size_t>(QueryError::kMessageSize));
	EXPECT_EQ(0u, msg.find("ambiguous layer name 'x': cells/a_rather_long_group_name_number_0/x"));
	EXPECT_EQ(0u, resolver.Error().mColumn);
}

TEST_F(QueryResolverTest, SameShortKeyOnAnotherGridIsNotAmbiguous)
{
	q.fx.Add(UploaderSchemaFixture::FloatEntry("a/x", 0, 1));
	q.fx.Add(UploaderSchemaFixture::FloatEntry("b/x", 0, 1));
	q.Publish();
	ASSERT_TRUE(q.AddLayer("la", &q.gridA, "cells/a/x"));
	ASSERT_TRUE(q.AddLayer("lb", &q.gridB, "cells/b/x"));
	ASSERT_TRUE(Run("x > 1")) << Msg();
	EXPECT_EQ(Dia::Core::StringCRC("la"), resolver.Binding(parser.RootIndex()).mLayerId);
}

TEST_F(QueryResolverTest, NullGridIsNoActiveGridError)
{
	Populate();
	ASSERT_TRUE(parser.Parse("temp > 1"));
	EXPECT_FALSE(resolver.Resolve("temp > 1", parser, q.registry, nullptr, q.service));
	EXPECT_STREQ("no active grid", resolver.Error().mMessage);
	EXPECT_EQ(0u, resolver.Error().mLength);
}

TEST_F(QueryResolverTest, ResolveClearsBindingsOfThePreviousQuery)
{
	Populate();
	ASSERT_TRUE(Run("temp > 1 and humidity < 2"));
	ASSERT_TRUE(resolver.Binding(1).mBound);
	ASSERT_TRUE(Run("temp > 1"));
	EXPECT_FALSE(resolver.Binding(1).mBound);
	EXPECT_FALSE(Run("temp > 1 and bogus < 2"));
	EXPECT_FALSE(resolver.Binding(1).mBound);	// the failing node is not bound
}

TEST_F(QueryResolverTest, VeryLongLayerNameIsAnErrorWithoutSuggestionOrOverflow)
{
	Populate();
	const std::string name(200, 'q');
	const std::string text = name + " > 1";
	EXPECT_FALSE(Run(text.c_str()));
	EXPECT_EQ(0u, resolver.Error().mColumn);
	EXPECT_EQ(200u, resolver.Error().mLength);
	EXPECT_FALSE(resolver.Error().HasSuggestion());
	EXPECT_LT(Msg().size(), static_cast<size_t>(QueryError::kMessageSize));
}

TEST_F(QueryResolverTest, VeryLongEnumIdIsAnErrorWithoutOverflow)
{
	Populate();
	const std::string text = "terrain = " + std::string(300, 'z');
	EXPECT_FALSE(Run(text.c_str()));
	EXPECT_EQ(10u, resolver.Error().mColumn);
	EXPECT_EQ(300u, resolver.Error().mLength);
	EXPECT_TRUE(MsgHas("unknown value")) << Msg();
}

TEST_F(QueryResolverTest, OrdinalOfEnumIdIsItsPositionNotAnIdHash)
{
	Populate();
	const char* const ids[] = { "grass", "water", "rock", "Sand" };
	for (uint32_t i = 0; i < 4; ++i)
	{
		const std::string text = std::string("terrain == ") + ids[i];
		ASSERT_TRUE(Run(text.c_str())) << Msg();
		EXPECT_DOUBLE_EQ(static_cast<double>(i), resolver.Binding(parser.RootIndex()).mThreshold) << ids[i];
	}
}
