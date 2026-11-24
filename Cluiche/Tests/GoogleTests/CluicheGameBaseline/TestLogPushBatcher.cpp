#include <gtest/gtest.h>

#include <Modules/LogPushBatcher.h>

using Cluiche::AppFlow::LogPushBatcher;

// LogPushBatcher: pure pacing for the debug panel's appendLogLines pushes (<= 1 per interval, newest lines kept when capped).

TEST(LogPushBatcher, NothingPendingDoesNotPush)
{
	LogPushBatcher b;
	EXPECT_FALSE(b.Evaluate(1000.0, 5, 5).push);
}

TEST(LogPushBatcher, FirstLinesPushImmediately)
{
	LogPushBatcher b;
	const LogPushBatcher::Plan p = b.Evaluate(0.0, 0, 3);
	EXPECT_TRUE(p.push);
	EXPECT_EQ(p.startCursor, 0u);
	EXPECT_EQ(p.take, 3u);
	EXPECT_EQ(p.dropped, 0u);
}

TEST(LogPushBatcher, SecondPushWaitsForInterval)
{
	LogPushBatcher b(250.0, 200);
	b.OnPushed(1000.0);
	EXPECT_FALSE(b.Evaluate(1100.0, 3, 10).push);
	EXPECT_FALSE(b.Evaluate(1249.0, 3, 10).push);
	EXPECT_TRUE(b.Evaluate(1250.0, 3, 10).push);
}

TEST(LogPushBatcher, QuietRingPushesPromptlyAfterInterval)
{
	LogPushBatcher b(250.0, 200);
	b.OnPushed(1000.0);
	const LogPushBatcher::Plan p = b.Evaluate(5000.0, 10, 11);
	EXPECT_TRUE(p.push);
	EXPECT_EQ(p.take, 1u);
}

TEST(LogPushBatcher, CapKeepsNewestAndCountsDropped)
{
	LogPushBatcher b(250.0, 200);
	const LogPushBatcher::Plan p = b.Evaluate(0.0, 100, 100 + 1000);
	EXPECT_TRUE(p.push);
	EXPECT_EQ(p.take, 200u);
	EXPECT_EQ(p.dropped, 800u);
	EXPECT_EQ(p.startCursor, 100u + 800u);   // resumes at the newest 200 lines
}

TEST(LogPushBatcher, ExactlyAtCapDropsNothing)
{
	LogPushBatcher b(250.0, 200);
	const LogPushBatcher::Plan p = b.Evaluate(0.0, 0, 200);
	EXPECT_EQ(p.take, 200u);
	EXPECT_EQ(p.dropped, 0u);
}

TEST(LogPushBatcher, CursorAheadOfTotalIsNothingPending)
{
	LogPushBatcher b;
	EXPECT_FALSE(b.Evaluate(0.0, 9, 4).push);
}

TEST(LogPushBatcher, DeclinedEvaluateDoesNotConsumeInterval)
{
	LogPushBatcher b(250.0, 200);
	EXPECT_FALSE(b.Evaluate(0.0, 4, 4).push);
	EXPECT_TRUE(b.Evaluate(1.0, 4, 5).push);   // no OnPushed yet, so no wait
}
