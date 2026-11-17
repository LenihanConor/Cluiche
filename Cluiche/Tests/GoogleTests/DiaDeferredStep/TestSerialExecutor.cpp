#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <utility>
#include <DiaDeferredStep/SerialExecutor.h>

using namespace Dia::DeferredStep;

TEST(DeferredStepSerialExecutorTest, CallsFnOnceWithFullRange)
{
	SerialExecutor executor;
	std::vector<std::pair<uint32_t, uint32_t>> calls;
	executor.ParallelFor(100, 8, [&](uint32_t b, uint32_t e) { calls.emplace_back(b, e); });
	ASSERT_EQ(calls.size(), 1u);
	EXPECT_EQ(calls[0].first, 0u);
	EXPECT_EQ(calls[0].second, 100u);
	EXPECT_EQ(executor.GetLastRangeCount(), 1u);
}

TEST(DeferredStepSerialExecutorTest, RunsOnCallingThread)
{
	SerialExecutor executor;
	std::thread::id seen;
	executor.ParallelFor(10, 1, [&](uint32_t, uint32_t) { seen = std::this_thread::get_id(); });
	EXPECT_EQ(seen, std::this_thread::get_id());
}

TEST(DeferredStepSerialExecutorTest, ZeroCountCallsNothing)
{
	SerialExecutor executor;
	int calls = 0;
	executor.ParallelFor(5, 1, [&](uint32_t, uint32_t) { ++calls; });
	executor.ParallelFor(0, 1, [&](uint32_t, uint32_t) { ++calls; });
	EXPECT_EQ(calls, 1);
	EXPECT_EQ(executor.GetLastRangeCount(), 0u);
}

TEST(DeferredStepSerialExecutorTest, MinRangeLargerThanCountStillOneCall)
{
	SerialExecutor executor;
	int calls = 0;
	uint32_t b = 99, e = 99;
	executor.ParallelFor(3, 1000, [&](uint32_t begin, uint32_t end) { ++calls; b = begin; e = end; });
	EXPECT_EQ(calls, 1);
	EXPECT_EQ(b, 0u);
	EXPECT_EQ(e, 3u);
	EXPECT_EQ(executor.GetLastRangeCount(), 1u);
}

TEST(DeferredStepSerialExecutorTest, RangeCountIsZeroBeforeAnyCall)
{
	SerialExecutor executor;
	EXPECT_EQ(executor.GetLastRangeCount(), 0u);
}

TEST(DeferredStepSerialExecutorTest, UsableThroughInterface)
{
	SerialExecutor serial;
	IParallelExecutor& executor = serial;
	uint32_t sum = 0;
	executor.ParallelFor(4, 1, [&](uint32_t b, uint32_t e) { for (uint32_t i = b; i < e; ++i) sum += i; });
	EXPECT_EQ(sum, 6u);
	EXPECT_EQ(executor.GetLastRangeCount(), 1u);
}
