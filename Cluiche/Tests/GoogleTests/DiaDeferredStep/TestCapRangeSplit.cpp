#include <gtest/gtest.h>

#include <DiaDeferredStep/RangeSplit.h>

#include <vector>

using namespace Dia::DeferredStep;

TEST(DeferredStepCapSplitTest, CoverageAndContiguity)
{
	for (uint32_t count = 0; count <= 70; ++count)
	{
		for (uint32_t rangeCount = 0; rangeCount <= 80; ++rangeCount)
		{
			const uint32_t effective = rangeCount == 0 ? 1u : rangeCount;
			std::vector<uint32_t> visits(count, 0);
			uint32_t expectedBegin = 0;
			for (uint32_t i = 0; i < effective; ++i)
			{
				uint32_t b = 99999, e = 99999;
				GetRangeBounds(count, rangeCount, i, b, e);
				EXPECT_EQ(b, expectedBegin) << count << "," << rangeCount << "," << i;
				EXPECT_LE(b, e);
				ASSERT_LE(e, count);
				for (uint32_t k = b; k < e; ++k) { ++visits[k]; }
				expectedBegin = e;
			}
			EXPECT_EQ(expectedBegin, count);
			for (uint32_t v : visits) { EXPECT_EQ(v, 1u); }
		}
	}
}

TEST(DeferredStepCapSplitTest, RemainderGoesToFirstRanges)
{
	uint32_t b, e;
	GetRangeBounds(10, 4, 0, b, e); EXPECT_EQ(e - b, 3u);
	GetRangeBounds(10, 4, 1, b, e); EXPECT_EQ(e - b, 3u);
	GetRangeBounds(10, 4, 2, b, e); EXPECT_EQ(e - b, 2u);
	GetRangeBounds(10, 4, 3, b, e); EXPECT_EQ(e - b, 2u);
}

TEST(DeferredStepCapSplitTest, CountLessThanRangeCountAndSingle)
{
	uint32_t b, e;
	GetRangeBounds(1, 4, 0, b, e); EXPECT_EQ(b, 0u); EXPECT_EQ(e, 1u);
	GetRangeBounds(1, 4, 1, b, e); EXPECT_EQ(b, e);
	GetRangeBounds(0, 3, 2, b, e); EXPECT_EQ(b, 0u); EXPECT_EQ(e, 0u);
	GetRangeBounds(5, 0, 0, b, e); EXPECT_EQ(b, 0u); EXPECT_EQ(e, 5u);
}

TEST(DeferredStepCapSplitTest, FormulaBoundaries)
{
	EXPECT_EQ(ComputeRangeCount(100, 100, 4, 4), 1u);
	EXPECT_EQ(ComputeRangeCount(101, 100, 4, 4), 2u);
	EXPECT_EQ(ComputeRangeCount(50, 100, 4, 4), 1u);
	EXPECT_EQ(ComputeRangeCount(0, 100, 4, 4), 1u);
	EXPECT_EQ(ComputeRangeCount(1000000, 100, 4, 4), 16u);
	EXPECT_EQ(ComputeRangeCount(1000000, 100, 1, 4), 4u);
	EXPECT_EQ(ComputeRangeCount(300, 100, 4, 4), 3u);
}

TEST(DeferredStepCapSplitTest, MinRangeZeroTreatedAsOne)
{
	EXPECT_EQ(ComputeRangeCount(5, 0, 4, 4), 5u);
	EXPECT_EQ(ComputeRangeCount(50, 0, 4, 4), 16u);
	EXPECT_EQ(ComputeRangeCount(0, 0, 4, 4), 1u);
}

TEST(DeferredStepCapSplitTest, NoOverflow)
{
	EXPECT_EQ(ComputeRangeCount(0xFFFFFFFFu, 1, 0xFFFFFFFFu, 0xFFFFFFFFu), 0xFFFFFFFFu);
	EXPECT_GE(ComputeRangeCount(10, 1, 0, 0), 1u);
	EXPECT_EQ(ComputeRangeCount(0xFFFFFFFFu, 1, 1, 1), 1u);
}
