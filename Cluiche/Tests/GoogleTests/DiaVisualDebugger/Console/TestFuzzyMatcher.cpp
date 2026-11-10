////////////////////////////////////////////////////////////////////////////////
// Filename: TestFuzzyMatcher.cpp
// Description: Unit tests for FuzzyMatcher (AC-5).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaVisualDebugger/Console/FuzzyMatcher.h>

using namespace Dia::VisualDebugger::Console;

TEST(FuzzyMatcherTest, AC5_RankOrderingOfSingleMatches)
{
	EXPECT_EQ(FuzzyMatcher::Match("bus", "bus", nullptr, 0), MatchRank::Exact);
	EXPECT_EQ(FuzzyMatcher::Match("bus", "bus_traffic", nullptr, 0), MatchRank::Prefix);
	EXPECT_EQ(FuzzyMatcher::Match("traf", "bus_traffic", nullptr, 0), MatchRank::Contains);
	EXPECT_EQ(FuzzyMatcher::Match("bstrfc", "bus_traffic.start", nullptr, 0), MatchRank::Subsequence);
	EXPECT_EQ(FuzzyMatcher::Match("xyz", "bus_traffic", nullptr, 0), MatchRank::None);
	EXPECT_EQ(FuzzyMatcher::Match("zb", "bus", nullptr, 0), MatchRank::None);
}

TEST(FuzzyMatcherTest, AC5_CaseInsensitive)
{
	EXPECT_EQ(FuzzyMatcher::Match("BUS", "bus", nullptr, 0), MatchRank::Exact);
	EXPECT_EQ(FuzzyMatcher::Match("bus", "BUS_Traffic", nullptr, 0), MatchRank::Prefix);
	EXPECT_EQ(FuzzyMatcher::Match("BSTRFC", "Bus_Traffic.Start", nullptr, 0), MatchRank::Subsequence);
}

TEST(FuzzyMatcherTest, AC5_MaskForPrefix)
{
	unsigned char mask[16] = { 9 };
	EXPECT_EQ(FuzzyMatcher::Match("bu", "bus_x", mask, 16), MatchRank::Prefix);
	EXPECT_EQ(mask[0], 1);
	EXPECT_EQ(mask[1], 1);
	EXPECT_EQ(mask[2], 0);
	EXPECT_EQ(mask[4], 0);
}

TEST(FuzzyMatcherTest, AC5_MaskForSubsequence)
{
	unsigned char mask[8] = {};
	EXPECT_EQ(FuzzyMatcher::Match("bsx", "bus_x", mask, 8), MatchRank::Subsequence);
	EXPECT_EQ(mask[0], 1);	// b
	EXPECT_EQ(mask[1], 0);	// u
	EXPECT_EQ(mask[2], 1);	// s
	EXPECT_EQ(mask[3], 0);	// _
	EXPECT_EQ(mask[4], 1);	// x
}

TEST(FuzzyMatcherTest, AC5_MaskSmallerThanCandidateDoesNotOverflow)
{
	unsigned char mask[2] = {};
	EXPECT_EQ(FuzzyMatcher::Match("bus", "bus_x", mask, 2), MatchRank::Prefix);
	EXPECT_EQ(mask[0], 1);
	EXPECT_EQ(mask[1], 1);
}

TEST(FuzzyMatcherTest, AC5_RankSortsByRankThenAlphabetical)
{
	const char* candidates[] = {
		"xx_bus_b",		// contains
		"b_u_s",		// subsequence
		"bus",			// exact
		"bus_zeta",		// prefix
		"Bus_alpha",	// prefix (case-insensitive alphabetical before zeta)
		"xx_bus_a",		// contains, alphabetical before xx_bus_b
		"nothing",		// none
	};
	unsigned out[16];
	unsigned n = FuzzyMatcher::Rank("bus", candidates, 7, out, 16);
	ASSERT_EQ(n, 6u);
	EXPECT_EQ(out[0], 2u);
	EXPECT_EQ(out[1], 4u);
	EXPECT_EQ(out[2], 3u);
	EXPECT_EQ(out[3], 5u);
	EXPECT_EQ(out[4], 0u);
	EXPECT_EQ(out[5], 1u);
}

TEST(FuzzyMatcherTest, AC5_RankRespectsMaxOutAndExcludesNonMatches)
{
	const char* candidates[] = { "bus_c", "bus_a", "bus_b", "zzz" };
	unsigned out[4];
	unsigned n = FuzzyMatcher::Rank("bus", candidates, 4, out, 2);
	ASSERT_EQ(n, 2u);
	EXPECT_EQ(out[0], 1u);
	EXPECT_EQ(out[1], 2u);
	EXPECT_EQ(FuzzyMatcher::Rank("qqq", candidates, 4, out, 4), 0u);
}
