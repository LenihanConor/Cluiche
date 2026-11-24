#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <DiaCore/CRC/StringCRC.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

// Feature 1 task 8: AC 10 - the published service is readable from other threads without locking.
// Structural note: ValueSchemaService / ValueSchemaRegistry expose only const lookups over an immutable
// published registry and include no mutex; this stress test hammers them from concurrent readers (run
// under ASan/UBSan via `dia check sanitizer`).

using namespace Dia::ValueSchema;
using Dia::ValueSchema::Testing::MakeValueInfo;
using Dia::Core::StringCRC;

namespace
{
	constexpr int kFormats = 20;
	constexpr int kPerFormat = 100;
	constexpr int kEventEvery = 10;
	constexpr int kReaders = 4;
	constexpr int kIterations = 20000;

	std::string FormatName(int f) { return "stress.format" + std::to_string(f); }
	std::string KeyName(int f, int i) { return "k" + std::to_string(f) + "_" + std::to_string(i); }
	bool IsEvent(int i) { return (i % kEventEvery) == 0; }
	std::string EventKeyName(int f, int i) { return "evt" + std::to_string(f) + "_" + std::to_string(i); }
	std::string EntryKey(int f, int i) { return IsEvent(i) ? EventKeyName(f, i) : KeyName(f, i); }
}

TEST(DiaValueSchemaStress, ConcurrentReadersSeeConsistentResults)
{
	ValueSchemaRegistry reg;
	for (int f = 0; f < kFormats; ++f)
	{
		const std::string fmt = FormatName(f);
		for (int i = 0; i < kPerFormat; ++i)
		{
			reg.Register(MakeValueInfo(fmt.c_str(), EntryKey(f, i).c_str(), IsEvent(i) ? ValueKind::Event : ValueKind::Parameter));
		}
	}
	reg.Publish();
	ValueSchemaService svc(reg);
	ASSERT_EQ(svc.Count(), static_cast<unsigned int>(kFormats * kPerFormat));

	// Precompute probe data on the main thread (readers only touch the service and these const vectors).
	std::vector<StringCRC> formats, fullKeys, keys, eventKeys;
	std::vector<int> formatOf;
	for (int f = 0; f < kFormats; ++f)
	{
		formats.push_back(StringCRC(FormatName(f).c_str()));
		for (int i = 0; i < kPerFormat; ++i)
		{
			fullKeys.push_back(StringCRC((FormatName(f) + "/" + EntryKey(f, i)).c_str()));
			keys.push_back(StringCRC(EntryKey(f, i).c_str()));
			formatOf.push_back(f);
			if (IsEvent(i))
				eventKeys.push_back(StringCRC(EntryKey(f, i).c_str()));
		}
	}
	const StringCRC unknownFull("stress.format0/does_not_exist");
	const StringCRC unknownFormat("stress.nope");
	const StringCRC unknownKey("does_not_exist");
	const unsigned int eventsPerFormat = kPerFormat / kEventEvery;

	std::atomic<bool> go{ false };
	std::atomic<unsigned int> failures{ 0 };

	auto reader = [&](int seed)
	{
		while (!go.load(std::memory_order_acquire)) { std::this_thread::yield(); }
		unsigned int bad = 0;
		const size_t n = fullKeys.size();
		for (int it = 0; it < kIterations; ++it)
		{
			const size_t idx = (static_cast<size_t>(it) * 7919u + static_cast<size_t>(seed) * 131u) % n;
			const int f = formatOf[idx];

			const ValueInfo* a = svc.Find(fullKeys[idx]);
			const ValueInfo* b = svc.Find(formats[f], keys[idx]);
			if (a == nullptr || a != b) ++bad;
			else if (a->mFor != StringCRC(FormatName(f).c_str())) ++bad;

			if (svc.Find(unknownFull) != nullptr) ++bad;
			if (svc.Find(formats[f], unknownKey) != nullptr) ++bad;
			if (svc.Count() != static_cast<unsigned int>(kFormats * kPerFormat)) ++bad;

			if ((it % 50) == 0)
			{
				const ValueInfoSpan span = svc.ForFormat(formats[f]);
				if (span.mCount != static_cast<unsigned int>(kPerFormat)) ++bad;
				else
				{
					for (unsigned int s = 0; s < span.mCount; ++s)
					{
						if (span.mItems[s] == nullptr || span.mItems[s]->mFor != formats[f]) { ++bad; break; }
					}
				}
				if (svc.ForFormat(unknownFormat).mCount != 0u) ++bad;

				const ValueInfo* e = svc.FindEvent(eventKeys[static_cast<size_t>(it) % eventKeys.size()]);
				if (e == nullptr || e->mKind != ValueKind::Event) ++bad;
				if (svc.FindEvent(unknownKey) != nullptr) ++bad;
				(void)eventsPerFormat;
			}
		}
		if (bad != 0) failures.fetch_add(bad);
	};

	std::vector<std::thread> threads;
	for (int t = 0; t < kReaders; ++t)
		threads.emplace_back(reader, t);
	go.store(true, std::memory_order_release);
	for (auto& t : threads)
		t.join();

	EXPECT_EQ(failures.load(), 0u);
}
