#include <gtest/gtest.h>
#include <DiaCore/Containers/HandlePool.h>

using namespace Dia::Core;

// ============================================================
// HandlePoolBasicTest
// ============================================================

TEST(HandlePoolBasicTest, EmptyPoolInvariantsHold)
{
	HandlePool<int, 8> pool;
	EXPECT_EQ(pool.GetSize(), 0u);
	EXPECT_EQ(pool.GetCapacity(), 8u);
	EXPECT_TRUE(pool.IsEmpty());
	EXPECT_FALSE(pool.IsFull());
}

// ============================================================
// HandlePoolAllocateTest
// ============================================================

TEST(HandlePoolAllocateTest, SingleAllocateReturnsValidHandle)
{
	HandlePool<int, 8> pool;
	Handle<int> h = pool.Allocate();
	EXPECT_TRUE(h.IsValid());
	EXPECT_NE(pool.Get(h), nullptr);
	EXPECT_EQ(pool.GetSize(), 1u);
}

TEST(HandlePoolAllocateTest, AllocateToCapacityFillsPool)
{
	HandlePool<int, 4> pool;
	pool.Allocate(); pool.Allocate(); pool.Allocate(); pool.Allocate();
	EXPECT_TRUE(pool.IsFull());
	EXPECT_EQ(pool.GetSize(), 4u);
}

TEST(HandlePoolAllocateTest, AllocateWhenFullPoolIsFull)
{
	HandlePool<int, 2> pool;
	pool.Allocate(); pool.Allocate();
	// Pool is full — confirmed via IsFull rather than triggering the debug assert
	EXPECT_TRUE(pool.IsFull());
}

// ============================================================
// HandlePoolAllocateArgsTest
// ============================================================

struct Vec2 { float x, y; Vec2(float x_, float y_) : x(x_), y(y_) {} };

TEST(HandlePoolAllocateArgsTest, ConstructsWithForwardedArgs)
{
	HandlePool<Vec2, 4> pool;
	Handle<Vec2> h = pool.Allocate(1.0f, 2.0f);
	ASSERT_NE(pool.Get(h), nullptr);
	EXPECT_FLOAT_EQ(pool.Get(h)->x, 1.0f);
	EXPECT_FLOAT_EQ(pool.Get(h)->y, 2.0f);
}

// ============================================================
// HandlePoolFreeTest
// ============================================================

TEST(HandlePoolFreeTest, FreeReturnsTrue)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate();
	EXPECT_TRUE(pool.Free(h));
}

TEST(HandlePoolFreeTest, FreeOnInvalidHandleReturnsFalse)
{
	HandlePool<int, 4> pool;
	EXPECT_FALSE(pool.Free(Handle<int>::Invalid()));
}

TEST(HandlePoolFreeTest, DoubleFreeReturnsFalse)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate();
	pool.Free(h);
	EXPECT_FALSE(pool.Free(h));
}

TEST(HandlePoolFreeTest, LiveCountDecrements)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate();
	EXPECT_EQ(pool.GetSize(), 1u);
	pool.Free(h);
	EXPECT_EQ(pool.GetSize(), 0u);
}

// ============================================================
// HandlePoolGenerationTest
// ============================================================

TEST(HandlePoolGenerationTest, StaleHandleGetReturnsNull)
{
	HandlePool<int, 4> pool;
	Handle<int> old = pool.Allocate();
	pool.Free(old);
	Handle<int> fresh = pool.Allocate();
	EXPECT_EQ(pool.Get(old), nullptr);
	EXPECT_FALSE(pool.IsValid(old));
	EXPECT_NE(pool.Get(fresh), nullptr);
	EXPECT_TRUE(pool.IsValid(fresh));
}

TEST(HandlePoolGenerationTest, GenerationDiffersBetweenCycles)
{
	HandlePool<int, 4> pool;
	Handle<int> h1 = pool.Allocate();
	pool.Free(h1);
	Handle<int> h2 = pool.Allocate();
	EXPECT_NE(h1.GetGeneration(), h2.GetGeneration());
}

// ============================================================
// HandlePoolFreeListReuseTest
// ============================================================

TEST(HandlePoolFreeListReuseTest, FreedSlotIsReused)
{
	HandlePool<int, 4> pool;
	Handle<int> h0 = pool.Allocate();
	Handle<int> h1 = pool.Allocate();
	Handle<int> h2 = pool.Allocate();
	pool.Free(h1);
	Handle<int> h3 = pool.Allocate();
	// h3 should reuse h1's slot index
	EXPECT_EQ(h3.GetIndex(), h1.GetIndex());
	EXPECT_NE(h3.GetGeneration(), h1.GetGeneration());
	(void)h0; (void)h2;
}

// ============================================================
// HandlePoolGetTest
// ============================================================

TEST(HandlePoolGetTest, DefaultHandleReturnsNull)
{
	HandlePool<int, 4> pool;
	EXPECT_EQ(pool.Get(Handle<int>()), nullptr);
}

TEST(HandlePoolGetTest, StaleHandleReturnsNull)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate();
	pool.Free(h);
	EXPECT_EQ(pool.Get(h), nullptr);
}

TEST(HandlePoolGetTest, LiveHandleReturnsValidPointer)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate();
	int* p = pool.Get(h);
	ASSERT_NE(p, nullptr);
	*p = 42;
	EXPECT_EQ(*pool.Get(h), 42);
}

TEST(HandlePoolGetTest, ConstOverloadWorks)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate();
	*pool.Get(h) = 7;
	const HandlePool<int, 4>& cpool = pool;
	const int* cp = cpool.Get(h);
	ASSERT_NE(cp, nullptr);
	EXPECT_EQ(*cp, 7);
}

// ============================================================
// HandlePoolIsValidTest
// ============================================================

TEST(HandlePoolIsValidTest, DefaultHandleInvalid)
{
	HandlePool<int, 4> pool;
	EXPECT_FALSE(pool.IsValid(Handle<int>()));
}

TEST(HandlePoolIsValidTest, AllocatedHandleValid)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate();
	EXPECT_TRUE(pool.IsValid(h));
}

TEST(HandlePoolIsValidTest, FreedHandleInvalid)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate();
	pool.Free(h);
	EXPECT_FALSE(pool.IsValid(h));
}

TEST(HandlePoolIsValidTest, OutOfRangeIndexInvalid)
{
	HandlePool<int, 4> pool;
	Handle<int> bad(100, 1);
	EXPECT_FALSE(pool.IsValid(bad));
}

// ============================================================
// HandlePoolForEachTest
// ============================================================

TEST(HandlePoolForEachTest, EmptyPoolVisitorNotCalled)
{
	HandlePool<int, 4> pool;
	int count = 0;
	pool.ForEach([&](Handle<int>, int&) { ++count; });
	EXPECT_EQ(count, 0);
}

TEST(HandlePoolForEachTest, VisitsAllLiveSlots)
{
	HandlePool<int, 4> pool;
	Handle<int> h0 = pool.Allocate(); *pool.Get(h0) = 1;
	Handle<int> h1 = pool.Allocate(); *pool.Get(h1) = 2;
	Handle<int> h2 = pool.Allocate(); *pool.Get(h2) = 3;
	int sum = 0;
	int count = 0;
	pool.ForEach([&](Handle<int>, int& v) { sum += v; ++count; });
	EXPECT_EQ(count, 3);
	EXPECT_EQ(sum, 6);
}

TEST(HandlePoolForEachTest, ConstForEachWorks)
{
	HandlePool<int, 4> pool;
	Handle<int> h = pool.Allocate(); *pool.Get(h) = 99;
	const HandlePool<int, 4>& cpool = pool;
	int val = 0;
	cpool.ForEach([&](Handle<int>, const int& v) { val = v; });
	EXPECT_EQ(val, 99);
}

TEST(HandlePoolForEachTest, SkipsFreedSlots)
{
	HandlePool<int, 4> pool;
	Handle<int> h0 = pool.Allocate(); *pool.Get(h0) = 10;
	Handle<int> h1 = pool.Allocate(); *pool.Get(h1) = 20;
	Handle<int> h2 = pool.Allocate(); *pool.Get(h2) = 30;
	pool.Free(h1);
	int count = 0;
	int sum = 0;
	pool.ForEach([&](Handle<int>, int& v) { sum += v; ++count; });
	EXPECT_EQ(count, 2);
	EXPECT_EQ(sum, 40);
}

// ============================================================
// HandlePoolDestructorTest
// ============================================================

static int sDestructorCount = 0;

struct DestructorCounter
{
	DestructorCounter() {}
	~DestructorCounter() { ++sDestructorCount; }
};

TEST(HandlePoolDestructorTest, DestructorCalledForLiveSlots)
{
	sDestructorCount = 0;
	{
		HandlePool<DestructorCounter, 4> pool;
		pool.Allocate();
		pool.Allocate();
		pool.Allocate();
	}
	EXPECT_EQ(sDestructorCount, 3);
}

TEST(HandlePoolDestructorTest, FreedSlotsNotDestructedTwice)
{
	sDestructorCount = 0;
	{
		HandlePool<DestructorCounter, 4> pool;
		Handle<DestructorCounter> h0 = pool.Allocate();
		Handle<DestructorCounter> h1 = pool.Allocate();
		pool.Free(h0); // destroys one — dtorCount = 1
		(void)h1;
	} // destroys remaining live slot — dtorCount = 2
	EXPECT_EQ(sDestructorCount, 2);
}

// ============================================================
// HandlePoolNonTrivialTypeTest
// ============================================================

struct NonTrivial
{
	int value;
	NonTrivial() : value(0) {}
	explicit NonTrivial(int v) : value(v) {}
	~NonTrivial() { value = -1; }
};

TEST(HandlePoolNonTrivialTypeTest, HeavyChurnNoCorruption)
{
	HandlePool<NonTrivial, 8> pool;
	Handle<NonTrivial> handles[8];

	for (int round = 0; round < 20; ++round)
	{
		uint32_t count = round % 8 + 1;
		for (uint32_t i = 0; i < count; ++i)
			handles[i] = pool.Allocate(round * 10 + (int)i);

		for (uint32_t i = 0; i < count; ++i)
		{
			ASSERT_NE(pool.Get(handles[i]), nullptr);
			EXPECT_EQ(pool.Get(handles[i])->value, round * 10 + (int)i);
		}

		for (uint32_t i = 0; i < count; ++i)
			pool.Free(handles[i]);

		EXPECT_EQ(pool.GetSize(), 0u);
	}
}
