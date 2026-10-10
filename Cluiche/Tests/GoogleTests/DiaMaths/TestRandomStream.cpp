#include <gtest/gtest.h>
#include <DiaMaths/Core/RandomStream.h>
#include <DiaMaths/Core/RandomStreamManager.h>
#include <DiaMaths/Core/Random.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>
#include <vector>
#include <thread>
#include <algorithm>

using namespace Dia::Maths;
using namespace Dia::Core;

//-----------------------------------------------------------------------------
// RandomStream: Stream Independence
//-----------------------------------------------------------------------------

TEST(RandomStream, StreamIndependence) {
	// Two streams with different seeds should produce different sequences
	RandomStream stream1(12345);
	RandomStream stream2(67890);

	const int kSampleCount = 100;
	std::vector<float> sequence1;
	std::vector<float> sequence2;

	for (int i = 0; i < kSampleCount; ++i) {
		sequence1.push_back(stream1.RandomUnit());
		sequence2.push_back(stream2.RandomUnit());
	}

	// Sequences should differ (extremely unlikely to be identical with 100 samples)
	ASSERT_NE(sequence1, sequence2);
}

//-----------------------------------------------------------------------------
// RandomStream: Determinism
//-----------------------------------------------------------------------------

TEST(RandomStream, Determinism) {
	// Same seed should produce identical sequences
	const unsigned int kSeed = 12345;
	const int kSampleCount = 100;

	std::vector<float> sequence1;
	{
		RandomStream stream(kSeed);
		for (int i = 0; i < kSampleCount; ++i) {
			sequence1.push_back(stream.RandomUnit());
		}
	}

	std::vector<float> sequence2;
	{
		RandomStream stream(kSeed);  // Same seed
		for (int i = 0; i < kSampleCount; ++i) {
			sequence2.push_back(stream.RandomUnit());
		}
	}

	ASSERT_EQ(sequence1, sequence2);
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - RandomInt
//-----------------------------------------------------------------------------

TEST(RandomStream, RandomInt) {
	RandomStream stream(12345);

	for (int i = 0; i < 10; ++i) {
		int value = stream.RandomInt();
		ASSERT_GE(value, 0);
		ASSERT_LE(value, INT_MAX);
	}
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - RandomRange (int)
//-----------------------------------------------------------------------------

TEST(RandomStream, RandomRangeInt) {
	RandomStream stream(12345);

	// Normal case
	for (int i = 0; i < 100; ++i) {
		int value = stream.RandomRange(10, 20);
		ASSERT_GE(value, 10);
		ASSERT_LE(value, 20);
	}

	// Edge case: min > max should swap
	for (int i = 0; i < 100; ++i) {
		int value = stream.RandomRange(20, 10);
		ASSERT_GE(value, 10);
		ASSERT_LE(value, 20);
	}

	// Edge case: min == max
	int value = stream.RandomRange(15, 15);
	ASSERT_EQ(value, 15);
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - RandomUnit
//-----------------------------------------------------------------------------

TEST(RandomStream, RandomUnit) {
	RandomStream stream(12345);

	for (int i = 0; i < 100; ++i) {
		float value = stream.RandomUnit();
		ASSERT_GE(value, 0.0f);
		ASSERT_LE(value, 1.0f);
	}
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - RandomRange (float)
//-----------------------------------------------------------------------------

TEST(RandomStream, RandomRangeFloat) {
	RandomStream stream(12345);

	// Normal case
	for (int i = 0; i < 100; ++i) {
		float value = stream.RandomRange(10.0f, 20.0f);
		ASSERT_GE(value, 10.0f);
		ASSERT_LE(value, 20.0f);
	}

	// Edge case: min > max should swap
	for (int i = 0; i < 100; ++i) {
		float value = stream.RandomRange(20.0f, 10.0f);
		ASSERT_GE(value, 10.0f);
		ASSERT_LE(value, 20.0f);
	}

	// Edge case: min == max
	float value = stream.RandomRange(15.0f, 15.0f);
	ASSERT_FLOAT_EQ(value, 15.0f);
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - RandomBilateral
//-----------------------------------------------------------------------------

TEST(RandomStream, RandomBilateral) {
	RandomStream stream(12345);

	for (int i = 0; i < 100; ++i) {
		float value = stream.RandomBilateral();
		ASSERT_GE(value, -1.0f);
		ASSERT_LE(value, 1.0f);
	}
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - RandomChance
//-----------------------------------------------------------------------------

TEST(RandomStream, RandomChance) {
	RandomStream stream(12345);

	// 0% probability should always be false
	for (int i = 0; i < 10; ++i) {
		ASSERT_FALSE(stream.RandomChance(0.0f));
	}

	// 100% probability should always be true
	for (int i = 0; i < 10; ++i) {
		ASSERT_TRUE(stream.RandomChance(1.0f));
	}

	// 50% probability should produce mix of true/false
	int trueCount = 0;
	const int kTrials = 1000;
	for (int i = 0; i < kTrials; ++i) {
		if (stream.RandomChance(0.5f)) {
			trueCount++;
		}
	}

	// With 1000 trials at 50%, expect roughly 500 true (allow 400-600 range)
	ASSERT_GE(trueCount, 400);
	ASSERT_LE(trueCount, 600);
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - RandomUnitVector
//-----------------------------------------------------------------------------

TEST(RandomStream, RandomUnitVector) {
	RandomStream stream(12345);

	for (int i = 0; i < 100; ++i) {
		Vector2D vec = stream.RandomUnitVector();

		// Should be unit length (magnitude close to 1.0)
		float magnitude = vec.Magnitude();
		ASSERT_NEAR(magnitude, 1.0f, 0.001f);
	}
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - WeightedRandom
//-----------------------------------------------------------------------------

TEST(RandomStream, WeightedRandom) {
	RandomStream stream(12345);

	// Normal case: weights {10, 30, 60}
	float weights[] = { 10.0f, 30.0f, 60.0f };
	std::vector<int> counts(3, 0);

	const int kTrials = 1000;
	for (int i = 0; i < kTrials; ++i) {
		int index = stream.WeightedRandom(weights, 3);
		ASSERT_GE(index, 0);
		ASSERT_LT(index, 3);
		counts[index]++;
	}

	// Index 2 (weight 60) should be selected most often
	ASSERT_GT(counts[2], counts[1]);
	ASSERT_GT(counts[1], counts[0]);

	// Edge case: zero total weight
	float zeroWeights[] = { 0.0f, 0.0f, 0.0f };
	int result = stream.WeightedRandom(zeroWeights, 3);
	ASSERT_EQ(result, -1);

	// Edge case: nullptr
	result = stream.WeightedRandom(nullptr, 3);
	ASSERT_EQ(result, -1);

	// Edge case: count <= 0
	result = stream.WeightedRandom(weights, 0);
	ASSERT_EQ(result, -1);
}

//-----------------------------------------------------------------------------
// RandomStream: API Coverage - Shuffle
//-----------------------------------------------------------------------------

TEST(RandomStream, Shuffle) {
	RandomStream stream(12345);

	// Shuffle an array
	int array[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	int original[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

	stream.Shuffle(array, 10);

	// Array should be different from original (extremely unlikely to be same after shuffle)
	bool isDifferent = false;
	for (int i = 0; i < 10; ++i) {
		if (array[i] != original[i]) {
			isDifferent = true;
			break;
		}
	}
	ASSERT_TRUE(isDifferent);

	// Array should contain same elements (just reordered)
	std::sort(array, array + 10);
	for (int i = 0; i < 10; ++i) {
		ASSERT_EQ(array[i], original[i]);
	}

	// Edge case: size 1 array (no change expected)
	int single[] = { 42 };
	stream.Shuffle(single, 1);
	ASSERT_EQ(single[0], 42);
}

//-----------------------------------------------------------------------------
// RandomStream: Thread Safety
//-----------------------------------------------------------------------------

TEST(RandomStream, ThreadSafety) {
	RandomStream stream(12345);

	// Launch multiple threads, each calling RandomUnit() many times
	std::vector<std::thread> threads;
	const int kThreadCount = 4;
	const int kCallsPerThread = 1000;

	for (int i = 0; i < kThreadCount; ++i) {
		threads.emplace_back([&stream]() {
			for (int j = 0; j < kCallsPerThread; ++j) {
				float value = stream.RandomUnit();
				// Just call it; if there's a race condition, TSan or crashes will detect
				(void)value;
			}
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	// If we reach here without crashing, thread safety is working
	SUCCEED();
}

//=============================================================================
// RandomStreamManager Tests
//=============================================================================

//-----------------------------------------------------------------------------
// RandomStreamManager: Create and Get
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, CreateAndGet) {
	RandomStreamManager mgr;

	// Create a stream
	bool created = mgr.CreateStream(StringCRC("test"), 12345);
	ASSERT_TRUE(created);

	// Stream should exist
	ASSERT_TRUE(mgr.HasStream(StringCRC("test")));

	// Get the stream
	RandomStream* stream = mgr.GetStream(StringCRC("test"));
	ASSERT_NE(stream, nullptr);

	// Stream should be usable
	float value = stream->RandomUnit();
	ASSERT_GE(value, 0.0f);
	ASSERT_LE(value, 1.0f);
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Get Non-Existent Stream
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, GetNonExistent) {
	RandomStreamManager mgr;

	// Get a stream that doesn't exist
	RandomStream* stream = mgr.GetStream(StringCRC("does_not_exist"));
	ASSERT_EQ(stream, nullptr);

	// HasStream should return false
	ASSERT_FALSE(mgr.HasStream(StringCRC("does_not_exist")));
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Double Create
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, DoubleCreate) {
	RandomStreamManager mgr;

	// Create a stream
	bool created1 = mgr.CreateStream(StringCRC("test"), 12345);
	ASSERT_TRUE(created1);

	// Get the stream and generate a value
	RandomStream* stream1 = mgr.GetStream(StringCRC("test"));
	ASSERT_NE(stream1, nullptr);
	float firstValue = stream1->RandomUnit();

	// Try to create again with different seed
	bool created2 = mgr.CreateStream(StringCRC("test"), 67890);
	ASSERT_FALSE(created2);  // Should fail

	// Original stream should be unchanged
	RandomStream* stream2 = mgr.GetStream(StringCRC("test"));
	ASSERT_EQ(stream1, stream2);  // Same pointer

	// Sequence should continue from original seed, not reset
	float secondValue = stream2->RandomUnit();
	ASSERT_NE(secondValue, firstValue);  // Different values (sequence continued)
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Destroy Stream
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, DestroyStream) {
	RandomStreamManager mgr;

	// Create a stream
	mgr.CreateStream(StringCRC("test"), 12345);
	ASSERT_TRUE(mgr.HasStream(StringCRC("test")));

	// Destroy it
	bool destroyed = mgr.DestroyStream(StringCRC("test"));
	ASSERT_TRUE(destroyed);

	// Stream should no longer exist
	ASSERT_FALSE(mgr.HasStream(StringCRC("test")));
	ASSERT_EQ(mgr.GetStream(StringCRC("test")), nullptr);
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Destroy Non-Existent Stream
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, DestroyNonExistent) {
	RandomStreamManager mgr;

	// Try to destroy a stream that doesn't exist
	bool destroyed = mgr.DestroyStream(StringCRC("does_not_exist"));
	ASSERT_FALSE(destroyed);
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Multiple Streams
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, MultipleStreams) {
	RandomStreamManager mgr;

	// Create multiple streams with different seeds
	mgr.CreateStream(StringCRC("stream1"), 11111);
	mgr.CreateStream(StringCRC("stream2"), 22222);
	mgr.CreateStream(StringCRC("stream3"), 33333);

	// All should exist
	ASSERT_TRUE(mgr.HasStream(StringCRC("stream1")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("stream2")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("stream3")));

	// Get all streams
	RandomStream* s1 = mgr.GetStream(StringCRC("stream1"));
	RandomStream* s2 = mgr.GetStream(StringCRC("stream2"));
	RandomStream* s3 = mgr.GetStream(StringCRC("stream3"));

	ASSERT_NE(s1, nullptr);
	ASSERT_NE(s2, nullptr);
	ASSERT_NE(s3, nullptr);

	// All streams should be independent (different pointers)
	ASSERT_NE(s1, s2);
	ASSERT_NE(s2, s3);
	ASSERT_NE(s1, s3);

	// Generate values from each - they should differ
	float v1 = s1->RandomUnit();
	float v2 = s2->RandomUnit();
	float v3 = s3->RandomUnit();

	// Extremely unlikely all three are identical
	ASSERT_TRUE(v1 != v2 || v2 != v3);
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Destructor Cleanup
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, DestructorCleanup) {
	// Create streams in a scope
	{
		RandomStreamManager mgr;
		mgr.CreateStream(StringCRC("temp1"), 12345);
		mgr.CreateStream(StringCRC("temp2"), 67890);

		ASSERT_TRUE(mgr.HasStream(StringCRC("temp1")));
		ASSERT_TRUE(mgr.HasStream(StringCRC("temp2")));

		// Manager destructor will be called here
	}

	// If we reach here without crashing, cleanup worked
	SUCCEED();
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Thread Safety (Registry Operations)
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, ThreadSafetyRegistry) {
	RandomStreamManager mgr;

	// Create initial streams
	for (int i = 0; i < 10; ++i) {
		std::string name = "stream" + std::to_string(i);
		mgr.CreateStream(StringCRC(name.c_str()), i * 1000);
	}

	// Launch threads that concurrently create, get, and destroy streams
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; ++t) {
		threads.emplace_back([&mgr, t]() {
			for (int i = 0; i < 100; ++i) {
				std::string name = "thread" + std::to_string(t) + "_" + std::to_string(i);

				// Create
				mgr.CreateStream(StringCRC(name.c_str()), i);

				// Get
				RandomStream* stream = mgr.GetStream(StringCRC(name.c_str()));
				if (stream) {
					stream->RandomUnit();  // Use the stream
				}

				// Destroy
				mgr.DestroyStream(StringCRC(name.c_str()));
			}
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	// If we reach here without crashing, thread safety is working
	SUCCEED();
}

//-----------------------------------------------------------------------------
// RandomStreamManager: CoW Use Case (8 streams)
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, CoWUseCase) {
	RandomStreamManager mgr;

	// Create the 8 streams CoW F-2 needs
	mgr.CreateStream(StringCRC("WLD"), 12345);
	mgr.CreateStream(StringCRC("A1"), 23456);
	mgr.CreateStream(StringCRC("B4"), 34567);
	mgr.CreateStream(StringCRC("B6"), 45678);
	mgr.CreateStream(StringCRC("C1"), 56789);
	mgr.CreateStream(StringCRC("C2"), 67890);
	mgr.CreateStream(StringCRC("B5"), 78901);
	mgr.CreateStream(StringCRC("C4"), 89012);

	// All should exist
	ASSERT_TRUE(mgr.HasStream(StringCRC("WLD")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("A1")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("B4")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("B6")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("C1")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("C2")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("B5")));
	ASSERT_TRUE(mgr.HasStream(StringCRC("C4")));

	// Get and use each stream
	RandomStream* wld = mgr.GetStream(StringCRC("WLD"));
	RandomStream* a1 = mgr.GetStream(StringCRC("A1"));

	ASSERT_NE(wld, nullptr);
	ASSERT_NE(a1, nullptr);

	// Generate values independently
	float wldValue = wld->RandomUnit();
	float a1Value = a1->RandomUnit();

	// Streams are independent
	ASSERT_NE(wldValue, a1Value);
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Capacity Limits (exceeding initial 16)
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, CapacityLimits) {
	RandomStreamManager mgr;

	// Create more than the initial capacity (16 streams)
	for (int i = 0; i < 32; ++i) {
		std::string name = "stream" + std::to_string(i);
		bool created = mgr.CreateStream(StringCRC(name.c_str()), i * 1000);
		ASSERT_TRUE(created);
		ASSERT_TRUE(mgr.HasStream(StringCRC(name.c_str())));
	}

	// All 32 streams should exist and be usable
	for (int i = 0; i < 32; ++i) {
		std::string name = "stream" + std::to_string(i);
		RandomStream* stream = mgr.GetStream(StringCRC(name.c_str()));
		ASSERT_NE(stream, nullptr);

		// Stream should be usable
		float value = stream->RandomUnit();
		ASSERT_GE(value, 0.0f);
		ASSERT_LE(value, 1.0f);
	}
}

//-----------------------------------------------------------------------------
// RandomStream: Edge Case Seeds
//-----------------------------------------------------------------------------

TEST(RandomStream, EdgeCaseSeeds) {
	// Seed = 0
	RandomStream stream0(0);
	float value0 = stream0.RandomUnit();
	ASSERT_GE(value0, 0.0f);
	ASSERT_LE(value0, 1.0f);

	// Seed = UINT_MAX
	RandomStream streamMax(UINT_MAX);
	float valueMax = streamMax.RandomUnit();
	ASSERT_GE(valueMax, 0.0f);
	ASSERT_LE(valueMax, 1.0f);

	// Different seeds should produce different sequences
	ASSERT_NE(value0, valueMax);

	// Seed = 0 should be deterministic (same as any other seed)
	RandomStream stream0_a(0);
	RandomStream stream0_b(0);
	float a1 = stream0_a.RandomUnit();
	float b1 = stream0_b.RandomUnit();
	ASSERT_EQ(a1, b1);
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Cross-Thread Independence
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, CrossThreadIndependence) {
	RandomStreamManager mgr;

	// Create two streams
	mgr.CreateStream(StringCRC("thread1"), 11111);
	mgr.CreateStream(StringCRC("thread2"), 22222);

	// Get the streams and generate values on separate threads
	std::vector<float> thread1Values;
	std::vector<float> thread2Values;

	std::thread t1([&mgr, &thread1Values]() {
		RandomStream* stream = mgr.GetStream(StringCRC("thread1"));
		for (int i = 0; i < 100; ++i) {
			thread1Values.push_back(stream->RandomUnit());
		}
	});

	std::thread t2([&mgr, &thread2Values]() {
		RandomStream* stream = mgr.GetStream(StringCRC("thread2"));
		for (int i = 0; i < 100; ++i) {
			thread2Values.push_back(stream->RandomUnit());
		}
	});

	t1.join();
	t2.join();

	// Both threads should have generated 100 values
	ASSERT_EQ(thread1Values.size(), 100u);
	ASSERT_EQ(thread2Values.size(), 100u);

	// Sequences should differ (different seeds)
	bool hasDifference = false;
	for (size_t i = 0; i < 100; ++i) {
		if (thread1Values[i] != thread2Values[i]) {
			hasDifference = true;
			break;
		}
	}
	ASSERT_TRUE(hasDifference);
}

//-----------------------------------------------------------------------------
// RandomStreamManager: Pointer Stability
//-----------------------------------------------------------------------------

TEST(RandomStreamManager, PointerStability) {
	RandomStreamManager mgr;

	// Create a stream
	mgr.CreateStream(StringCRC("stable"), 12345);

	// Get the pointer multiple times
	RandomStream* ptr1 = mgr.GetStream(StringCRC("stable"));
	RandomStream* ptr2 = mgr.GetStream(StringCRC("stable"));
	RandomStream* ptr3 = mgr.GetStream(StringCRC("stable"));

	// All pointers should be the same
	ASSERT_EQ(ptr1, ptr2);
	ASSERT_EQ(ptr2, ptr3);

	// Use the stream between gets - pointer should remain stable
	ptr1->RandomUnit();
	RandomStream* ptr4 = mgr.GetStream(StringCRC("stable"));
	ASSERT_EQ(ptr1, ptr4);
}

//-----------------------------------------------------------------------------
// Backward Compatibility: Existing Random:: API
//-----------------------------------------------------------------------------

TEST(BackwardCompatibility, RandomNamespaceStillWorks) {
	// The old Random:: namespace functions should still work
	// This verifies we didn't break the existing API

	// Initialize (optional)
	Random::Initialize(54321);

	// All existing functions should work
	int randomInt = Random::RandomInt();
	ASSERT_GE(randomInt, 0);

	int rangeInt = Random::RandomRange(1, 10);
	ASSERT_GE(rangeInt, 1);
	ASSERT_LE(rangeInt, 10);

	float unit = Random::RandomUnit();
	ASSERT_GE(unit, 0.0f);
	ASSERT_LE(unit, 1.0f);

	float rangeFloat = Random::RandomRange(0.0f, 100.0f);
	ASSERT_GE(rangeFloat, 0.0f);
	ASSERT_LE(rangeFloat, 100.0f);

	float bilateral = Random::RandomBilateral();
	ASSERT_GE(bilateral, -1.0f);
	ASSERT_LE(bilateral, 1.0f);

	bool chance = Random::RandomChance(0.5f);
	// Just verify it doesn't crash - result is non-deterministic
	(void)chance;

	Vector2D unitVec = Random::RandomUnitVector();
	float magnitude = unitVec.Magnitude();
	ASSERT_NEAR(magnitude, 1.0f, 0.001f);

	float weights[] = { 10.0f, 20.0f, 30.0f };
	int index = Random::WeightedRandom(weights, 3);
	ASSERT_GE(index, 0);
	ASSERT_LT(index, 3);

	int array[] = { 1, 2, 3, 4, 5 };
	Random::Shuffle(array, 5);
	// Just verify it doesn't crash - result is non-deterministic
}
