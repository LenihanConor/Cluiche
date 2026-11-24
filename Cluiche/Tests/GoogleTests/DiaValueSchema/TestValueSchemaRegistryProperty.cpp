#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include "ValueSchemaTestSupport.h"
#include <map>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// ValueSchemaRegistry property / edge tests (seeded), including real CRC collisions found by brute force.

using namespace Dia::ValueSchema;
using namespace Dia::ValueSchema::Testing;
using Dia::Core::StringCRC;

namespace
{
	// Finds two distinct strings with the same 32-bit StringCRC (birthday search over seeded random strings).
	// Sequential strings never collide: CRC-32 is linear and detects every difference confined to 32 bits.
	bool FindCollidingPair(const std::string& prefix, std::string& a, std::string& b)
	{
		std::mt19937 rng(1234);
		std::unordered_map<unsigned, std::string> seen;
		for (unsigned i = 0; i < 1000000; ++i)
		{
			std::string s = prefix;
			for (int c = 0; c < 8; ++c) { s += static_cast<char>('a' + rng() % 26); }
			const unsigned h = StringCRC(s.c_str()).Value();
			auto it = seen.find(h);
			if (it != seen.end() && it->second != s) { a = it->second; b = s; return true; }
			seen[h] = s;
		}
		return false;
	}

	ValueInfo Info(const std::string& forName, const std::string& key, ValueKind kind = ValueKind::Parameter, const char* label = "")
	{
		ValueInfo v = MakeValueInfo(forName.c_str(), key.c_str(), kind);
		v.mLabel = label;
		return v;
	}

	std::string Full(const std::string& f, const std::string& k) { return f + "/" + k; }
}

TEST(DiaValueSchemaRegistryProperty, RandomInsertSequencesKeepAllIndexesConsistent)
{
	for (unsigned seed : { 1234u, 99u, 7u })
	{
		SCOPED_TRACE(seed);
		std::mt19937 rng(seed);
		const char* fors[] = { "alpha", "beta", "gamma.delta", "e" };

		ValueSchemaRegistry reg;
		std::map<std::string, std::string> firstLabel;			// full key -> label of the first registration
		std::map<std::string, std::vector<std::string>> order;	// for -> full keys in insertion order
		std::set<std::string> eventKeys;
		unsigned okCount = 0;

		for (int i = 0; i < 600; ++i)
		{
			const std::string f = fors[rng() % 4];
			const std::string k = "key" + std::to_string(rng() % 40);
			const bool isEvent = (rng() % 7 == 0);
			const std::string label = "L" + std::to_string(i);
			const std::string full = Full(f, k);

			RegisterResult expected = RegisterResult::Ok;
			if (firstLabel.count(full)) { expected = RegisterResult::DuplicateKey; }
			else if (isEvent && eventKeys.count(k)) { expected = RegisterResult::DuplicateEventKey; }

			const RegisterResult got = reg.Register(Info(f, k, isEvent ? ValueKind::Event : ValueKind::Parameter, label.c_str()));
			ASSERT_EQ(got, expected) << full << " #" << i;
			if (got == RegisterResult::Ok)
			{
				++okCount;
				firstLabel[full] = label;
				order[f].push_back(full);
				if (isEvent) { eventKeys.insert(k); }
			}
		}

		EXPECT_EQ(reg.Count(), okCount);
		unsigned total = 0;
		for (const auto& kv : order)
		{
			const ValueInfoSpan span = reg.ForFormat(StringCRC(kv.first.c_str()));
			ASSERT_EQ(span.mCount, kv.second.size()) << kv.first;
			total += span.mCount;
			for (unsigned i = 0; i < span.mCount; ++i)
			{
				const ValueInfo* item = span.mItems[i];
				EXPECT_STREQ(item->mFullKey.AsCStr(), kv.second[i].c_str()) << "ForFormat order == insertion order";
				EXPECT_EQ(reg.Find(StringCRC(item->mFullKey.AsCStr())), item);
				EXPECT_EQ(reg.Find(StringCRC(item->mFor.AsCStr()), StringCRC(item->mKey.AsCStr())), item);
				EXPECT_STREQ(item->mLabel.AsCStr(), firstLabel[kv.second[i]].c_str()) << "duplicates leave the first";
			}
		}
		EXPECT_EQ(total, okCount);
		for (const std::string& ek : eventKeys)
		{
			const ValueInfo* ev = reg.FindEvent(StringCRC(ek.c_str()));
			ASSERT_NE(ev, nullptr);
			EXPECT_EQ(ev->mKind, ValueKind::Event);
		}
	}
}

TEST(DiaValueSchemaRegistryProperty, EmptyRegistry)
{
	ValueSchemaRegistry reg;
	EXPECT_EQ(reg.Count(), 0u);
	EXPECT_FALSE(reg.IsPublished());
	EXPECT_EQ(reg.Find(StringCRC("a/b")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("a"), StringCRC("b")), nullptr);
	EXPECT_EQ(reg.FindEvent(StringCRC("b")), nullptr);
	const ValueInfoSpan span = reg.ForFormat(StringCRC("a"));
	EXPECT_EQ(span.mCount, 0u);
	EXPECT_EQ(span.mItems, nullptr);
	reg.Publish();
	EXPECT_TRUE(reg.IsPublished());
	EXPECT_EQ(reg.Count(), 0u);
}

TEST(DiaValueSchemaRegistryProperty, PublishTwiceIsIdempotentAndBlocksRegistration)
{
	ValueSchemaRegistry reg;
	ASSERT_EQ(reg.Register(Info("a", "k")), RegisterResult::Ok);
	reg.Publish();
	reg.Publish();
	EXPECT_TRUE(reg.IsPublished());
	EXPECT_EQ(reg.Register(Info("a", "other")), RegisterResult::AlreadyPublished);
	EXPECT_EQ(reg.Register(Info("a", "k")), RegisterResult::AlreadyPublished);	// even a duplicate reports AlreadyPublished
	EXPECT_EQ(reg.Register(Info("a", "x"), StringCRC("forced")), RegisterResult::AlreadyPublished);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_NE(reg.Find(StringCRC("a/k")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("a/other")), nullptr);
}

TEST(DiaValueSchemaRegistryProperty, LongestFullKeyRegistersAndFinds)
{
	const std::string f(63, 'f');
	const std::string k(127, 'k');
	ValueSchemaRegistry reg;
	ASSERT_EQ(reg.Register(Info(f, k)), RegisterResult::Ok);
	const std::string full = Full(f, k);
	ASSERT_EQ(full.size(), 191u);
	const ValueInfo* v = reg.Find(StringCRC(full.c_str()));
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mFullKey.AsCStr(), full.c_str());
	EXPECT_EQ(reg.Find(StringCRC(f.c_str()), StringCRC(k.c_str())), v);
}

TEST(DiaValueSchemaRegistryProperty, FullKeyHashUsesTheWholeString)
{
	// StringCRC stores only 63 characters for AsChar(), but the CRC covers the whole string: two keys that share
	// the first 63 characters must stay distinct.
	const std::string common(100, 'a');
	ValueSchemaRegistry reg;
	ASSERT_EQ(reg.Register(Info("f", common + "x")), RegisterResult::Ok);
	ASSERT_EQ(reg.Register(Info("f", common + "y")), RegisterResult::Ok);
	EXPECT_EQ(reg.Count(), 2u);
	const ValueInfo* x = reg.Find(StringCRC(Full("f", common + "x").c_str()));
	const ValueInfo* y = reg.Find(StringCRC(Full("f", common + "y").c_str()));
	ASSERT_NE(x, nullptr);
	ASSERT_NE(y, nullptr);
	EXPECT_NE(x, y);
	EXPECT_EQ(reg.Find(StringCRC("f"), StringCRC((common + "x").c_str())), x);
	EXPECT_EQ(reg.Find(StringCRC("f"), StringCRC((common + "y").c_str())), y);
}

TEST(DiaValueSchemaRegistryProperty, PinEmptyForAndEmptyKeyRegister)
{
	ValueSchemaRegistry reg;
	EXPECT_EQ(reg.Register(Info("", "")), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(Info("", "")), RegisterResult::DuplicateKey);
	EXPECT_EQ(reg.Register(Info("f", "")), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(Info("", "k")), RegisterResult::Ok);
	EXPECT_EQ(reg.Count(), 3u);
	EXPECT_NE(reg.Find(StringCRC("/")), nullptr);
	EXPECT_NE(reg.Find(StringCRC("f/")), nullptr);
	EXPECT_NE(reg.Find(StringCRC("/k")), nullptr);
}

TEST(DiaValueSchemaRegistryProperty, StoredCopyIsIndependentOfCaller)
{
	ValueSchemaRegistry reg;
	ValueInfo v = Info("f", "k", ValueKind::Parameter, "orig");
	v.mUi["g"] = "x";
	ASSERT_EQ(reg.Register(v), RegisterResult::Ok);
	v.mLabel = "changed";
	v.mUi["g"] = "changed";
	const ValueInfo* stored = reg.Find(StringCRC("f/k"));
	ASSERT_NE(stored, nullptr);
	EXPECT_STREQ(stored->mLabel.AsCStr(), "orig");
	EXPECT_STREQ(stored->mUi["g"].asCString(), "x");
	EXPECT_NE(stored, &v);
}

// ---- real hash collisions (suspected defect (f)) -------------------------------------------------------------------

TEST(DiaValueSchemaRegistryProperty, TwoKeysWithTheSameCrcInOneFormatAreACollision)
{
	// Equal-length keys with equal CRCs also have equal full-key CRCs ("for/" is a common prefix and CRC-32 is
	// affine), so this one is caught by the full-key table; it must stay a clean HashCollision.
	std::string a, b;
	ASSERT_TRUE(FindCollidingPair("key_", a, b));
	ASSERT_NE(a, b);
	ASSERT_EQ(StringCRC(a.c_str()).Value(), StringCRC(b.c_str()).Value());

	ValueSchemaRegistry reg;
	ASSERT_EQ(reg.Register(Info("f", a, ValueKind::Parameter, "first")), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(Info("f", b, ValueKind::Parameter, "second")), RegisterResult::HashCollision);
	EXPECT_EQ(reg.Count(), 1u);
	const ValueInfo* byFull = reg.Find(StringCRC(Full("f", a).c_str()));
	ASSERT_NE(byFull, nullptr);
	EXPECT_EQ(byFull, reg.Find(StringCRC("f"), StringCRC(a.c_str())));
	EXPECT_STREQ(byFull->mLabel.AsCStr(), "first");
	EXPECT_EQ(reg.ForFormat(StringCRC("f")).mCount, 1u);
}

TEST(DiaValueSchemaRegistryProperty, KeyCrcCollisionWithDifferentFullKeyHashIsRejectedNotOverwritten)
{
	// Same key CRC inside one format but a different full-key hash (forced through the test seam): previously the
	// per-format table slot was silently overwritten, so Find(for, key) and Find(fullKey) disagreed.
	std::string a, b;
	ASSERT_TRUE(FindCollidingPair("key_", a, b));
	ValueSchemaRegistry reg;
	ASSERT_EQ(reg.Register(Info("f", a, ValueKind::Parameter, "first")), RegisterResult::Ok);
	const StringCRC other("some-other-full-hash");
	EXPECT_EQ(reg.Register(Info("f", b, ValueKind::Parameter, "second"), other), RegisterResult::HashCollision);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_EQ(reg.Find(other), nullptr);
	const ValueInfo* byPair = reg.Find(StringCRC("f"), StringCRC(a.c_str()));
	ASSERT_NE(byPair, nullptr);
	EXPECT_STREQ(byPair->mLabel.AsCStr(), "first");
	EXPECT_EQ(byPair, reg.Find(StringCRC(Full("f", a).c_str())));
	const ValueInfoSpan span = reg.ForFormat(StringCRC("f"));
	ASSERT_EQ(span.mCount, 1u);
	EXPECT_EQ(span.mItems[0], byPair);

	// The same key registered again under a different forced hash is a duplicate, not a collision.
	EXPECT_EQ(reg.Register(Info("f", a, ValueKind::Parameter, "again"), other), RegisterResult::DuplicateKey);
	EXPECT_EQ(reg.Count(), 1u);
}

TEST(DiaValueSchemaRegistryProperty, SameKeyCrcInDifferentFormatsIsNotACollision)
{
	std::string a, b;
	ASSERT_TRUE(FindCollidingPair("key_", a, b));
	ValueSchemaRegistry reg;
	EXPECT_EQ(reg.Register(Info("f1", a)), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(Info("f2", b)), RegisterResult::Ok);	// different formats, different tables
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaRegistryProperty, EventKeysWithTheSameCrcAreACollisionNotADuplicate)
{
	std::string a, b;
	ASSERT_TRUE(FindCollidingPair("evt_", a, b));
	ValueSchemaRegistry reg;
	ASSERT_EQ(reg.Register(Info("f1", a, ValueKind::Event, "first")), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(Info("f2", b, ValueKind::Event, "second")), RegisterResult::HashCollision);
	EXPECT_EQ(reg.Count(), 1u);
	ASSERT_NE(reg.FindEvent(StringCRC(a.c_str())), nullptr);
	EXPECT_STREQ(reg.FindEvent(StringCRC(a.c_str()))->mLabel.AsCStr(), "first");
	// The genuine duplicate keeps its own verdict.
	EXPECT_EQ(reg.Register(Info("f2", a, ValueKind::Event)), RegisterResult::DuplicateEventKey);
}

TEST(DiaValueSchemaRegistryProperty, FormatsWithTheSameCrcAreACollision)
{
	std::string a, b;
	ASSERT_TRUE(FindCollidingPair("fmt_", a, b));
	ValueSchemaRegistry reg;
	ASSERT_EQ(reg.Register(Info(a, "k1")), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(Info(b, "k2")), RegisterResult::HashCollision);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_EQ(reg.ForFormat(StringCRC(a.c_str())).mCount, 1u);
}

TEST(DiaValueSchemaRegistryProperty, ForcedFullKeyHashAcrossManyCollisionsKeepsFirstOnly)
{
	ValueSchemaRegistry reg;
	const StringCRC forced("forced");
	EXPECT_EQ(reg.Register(Info("a", "k0"), forced), RegisterResult::Ok);
	for (int i = 1; i < 20; ++i)
	{
		EXPECT_EQ(reg.Register(Info("a", "k" + std::to_string(i)), forced), RegisterResult::HashCollision);
	}
	EXPECT_EQ(reg.Register(Info("a", "k0"), forced), RegisterResult::DuplicateKey);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_EQ(reg.ForFormat(StringCRC("a")).mCount, 1u);
}
