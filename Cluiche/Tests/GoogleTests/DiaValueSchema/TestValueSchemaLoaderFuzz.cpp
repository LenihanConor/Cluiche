#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/CategoricalColours.h>
#include "ValueSchemaTestSupport.h"
#include <cstdio>
#include <random>
#include <string>
#include <vector>

// Loader property / fuzz-lite tests (fixed seed): SplitRecordPath round trip, seeded random byte mutations of every
// good conformance fixture (never crash, invariants hold), determinism, and a golden dump.

using namespace Dia::ValueSchema;
using namespace Dia::Core::Containers;

namespace
{
	const char* kFile = "fuzz.diavalues";

	std::string Num(double d)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.17g", d);
		return buf;
	}

	std::string Join(const ValueList<String128>& l)
	{
		std::string s;
		for (unsigned i = 0; i < l.Size(); ++i) { if (i) { s += ","; } s += l[i].AsCStr(); }
		return s;
	}

	// Canonical dump of everything the loader produces (used for determinism and the golden test).
	std::string Dump(const LoadResult& r)
	{
		std::string out;
		out += std::string("ok=") + (r.mOk ? "1" : "0") + " for=" + r.mFor.AsCStr() + " entries=" + std::to_string(r.mEntries.Size())
			+ " warnings=" + std::to_string(r.mWarnings.Size()) + "\n";
		if (!r.mOk)
		{
			out += std::string("fail file=") + r.mFailure.mFile.AsCStr() + " entry=" + r.mFailure.mEntry.AsCStr() + " reason=" + r.mFailure.mReason.AsCStr() + "\n";
		}
		for (unsigned i = 0; i < r.mWarnings.Size(); ++i) { out += std::string("warn ") + r.mWarnings[i].AsCStr() + "\n"; }
		Json::FastWriter w;
		for (unsigned i = 0; i < r.mEntries.Size(); ++i)
		{
			const ValueInfo& v = r.mEntries[i];
			out += "[" + std::to_string(i) + "] key=" + v.mKey.AsCStr() + " for=" + v.mFor.AsCStr() + " kind=" + std::to_string(static_cast<int>(v.mKind))
				+ " type=" + std::to_string(static_cast<int>(v.mType)) + " label=" + v.mLabel.AsCStr() + " desc=" + v.mDescription.AsCStr()
				+ " hint=" + v.mHint.AsCStr() + " unit=" + v.mUnit.AsCStr()
				+ " min=" + (v.mHasMin ? Num(v.mMin) : "-") + " max=" + (v.mHasMax ? Num(v.mMax) : "-")
				+ " default=" + (v.mHasDefault ? Num(v.mDefaultNumber) + "/" + v.mDefaultString.AsCStr() : "-")
				+ " affects=" + Join(v.mAffects) + " related=" + Join(v.mRelated) + " source=" + v.mSource.AsCStr()
				+ " unique=" + (v.mUnique ? "1" : "0") + " required=" + (v.mRequired ? "1" : "0") + " ref=" + v.mRef.AsCStr()
				+ " keyRef=" + v.mMapKeyRef.AsCStr() + " valueType=" + std::to_string(static_cast<int>(v.mMapValueType))
				+ " file=" + v.mSourceFile.AsCStr() + " segs=";
			for (unsigned s = 0; s < v.mPathSegments.Size(); ++s) { if (s) { out += "|"; } out += v.mPathSegments[s].AsCStr(); }
			out += " ui=" + w.write(v.mUi);
			out.pop_back();	// FastWriter appends a newline
			out += " history=" + w.write(v.mHistory);
			out.pop_back();
			out += "\n";
			for (unsigned e = 0; e < v.mEnumValues.Size(); ++e)
			{
				const EnumValue& ev = v.mEnumValues[e];
				out += std::string("  enum ") + ev.mId.AsCStr() + "|" + ev.mLabel.AsCStr() + "|" + ev.mDescription.AsCStr() + "|" + ev.mColour.AsCStr() + "|" + (ev.mHasColour ? "given" : "assigned") + "\n";
			}
			for (unsigned p = 0; p < v.mPayload.Size(); ++p)
			{
				const PayloadField& f = v.mPayload[p];
				out += std::string("  payload ") + f.mName.AsCStr() + "|" + std::to_string(static_cast<int>(f.mType)) + "|" + f.mDescription.AsCStr() + "|" + f.mUnit.AsCStr() + "\n";
			}
			for (unsigned p = 0; p < v.mVocabulary.Size(); ++p)
			{
				const VocabularyEntry& f = v.mVocabulary[p];
				out += std::string("  vocab ") + f.mId.AsCStr() + "|" + f.mLabel.AsCStr() + "|" + f.mDescription.AsCStr() + "|" + f.mUnit.AsCStr() + "\n";
			}
		}
		return out;
	}

	std::filesystem::path ConformanceDir()
	{
		const std::filesystem::path expected = VsTest::FindUpwards(__FILE__, "Dia/DiaValueSchema/Testing/Conformance/expected.json");
		return expected.empty() ? expected : expected.parent_path();
	}

	// Names of the fixtures expected to load.
	std::vector<std::string> GoodFixtures(const std::filesystem::path& dir)
	{
		std::vector<std::string> names;
		std::string text;
		if (!VsTest::ReadFileBytes(dir / "expected.json", text)) { return names; }
		Json::Value expected;
		Json::Reader reader;
		if (!reader.parse(text, expected)) { return names; }
		for (const std::string& n : expected["cases"].getMemberNames())
		{
			if (expected["cases"][n]["ok"].asBool()) { names.push_back(n); }
		}
		return names;
	}

	std::string Mutate(const std::string& src, std::mt19937& rng)
	{
		std::string s = src;
		if (s.empty()) { return s; }
		const unsigned ops = 1 + rng() % 3;
		for (unsigned k = 0; k < ops && !s.empty(); ++k)
		{
			const size_t pos = rng() % s.size();
			switch (rng() % 6)
			{
			case 0:	s[pos] = static_cast<char>(s[pos] ^ (1 << (rng() % 8))); break;				// flip a bit
			case 1:	s.erase(pos, 1 + rng() % 8); break;											// delete a run
			case 2:	s.insert(pos, 1, static_cast<char>(rng() % 256)); break;					// insert a random byte
			case 3:	s.resize(pos); break;														// truncate
			case 4:	s.insert(pos, s.substr(pos, 1 + rng() % 32)); break;						// duplicate a chunk
			default: s[pos] = "{}[]\",:0-.eE\\ntf"[rng() % 17]; break;							// structural character
			}
		}
		return s;
	}

	void CheckInvariants(const LoadResult& r, const std::string& label)
	{
		if (!r.mOk)
		{
			ASSERT_EQ(r.mEntries.Size(), 0u) << label;
			ASSERT_STREQ(r.mFailure.mFile.AsCStr(), kFile) << label;
			ASSERT_GT(strlen(r.mFailure.mReason.AsCStr()), 0u) << label;
			ASSERT_LT(strlen(r.mFailure.mReason.AsCStr()), 512u) << label;
			ASSERT_LT(strlen(r.mFailure.mEntry.AsCStr()), 128u) << label;
			return;
		}
		for (unsigned i = 0; i < r.mEntries.Size(); ++i)
		{
			const ValueInfo& v = r.mEntries[i];
			ASSERT_STREQ(v.mSourceFile.AsCStr(), kFile) << label;
			ASSERT_STREQ(v.mFor.AsCStr(), r.mFor.AsCStr()) << label;
			ASSERT_LE(v.mPathSegments.Size(), 16u) << label;
			ASSERT_LE(strlen(v.mKey.AsCStr()), 127u) << label;
			if (v.mPayload.Size() > 0 || v.mVocabulary.Size() > 0) { ASSERT_EQ(v.mKind, ValueKind::Event) << label; }
			if (v.mType == ValueType::Ref || v.mType == ValueType::RefList) { ASSERT_TRUE(v.mMapKeyRef.IsEmpty()) << label; }
			if (v.mType == ValueType::Map)
			{
				ASSERT_NE(v.mMapValueType, ValueType::Ref) << label;
				ASSERT_NE(v.mMapValueType, ValueType::RefList) << label;
				ASSERT_NE(v.mMapValueType, ValueType::Map) << label;
			}
			ASSERT_TRUE(v.mUi.isNull() || v.mUi.isObject()) << label;
			ASSERT_TRUE(v.mHistory.isNull() || v.mHistory.isObject()) << label;
			for (unsigned e = 0; e < v.mEnumValues.Size(); ++e)
			{
				ASSERT_GT(strlen(v.mEnumValues[e].mColour.AsCStr()), 0u) << label;
				if (!v.mEnumValues[e].mHasColour) { ASSERT_STREQ(v.mEnumValues[e].mColour.AsCStr(), GetCategoricalColour(e)) << label; }
			}
		}
	}
}

TEST(DiaValueSchemaLoaderFuzz, SplitRecordPathRoundTripForRandomKeys)
{
	std::mt19937 rng(1234);
	const char alphabet[] = { 'a', 'b', 'z', '.', '[', ']' };
	for (int iter = 0; iter < 5000; ++iter)
	{
		std::string key;
		const unsigned len = rng() % 40;
		for (unsigned i = 0; i < len; ++i) { key += alphabet[rng() % 6]; }

		ValueList<String64> segs;
		segs.Reserve(64);
		ASSERT_TRUE(SplitRecordPath(key.c_str(), segs)) << key;	// segments are at most 40 characters long here

		// Segments never contain '.' or the sequence "[]"
		// (except as the segment "[]" itself), and concatenating them equals the key with dots removed.
		std::string joinedSegs, keyNoDots;
		for (unsigned i = 0; i < segs.Size(); ++i)
		{
			const std::string s = segs[i].AsCStr();
			ASSERT_EQ(s.find('.'), std::string::npos) << key;
			if (s != "[]") { ASSERT_EQ(s.find("[]"), std::string::npos) << key; }
			joinedSegs += s;
		}
		for (char c : key) { if (c != '.') { keyNoDots += c; } }
		ASSERT_EQ(joinedSegs, keyNoDots) << key;
	}
}

TEST(DiaValueSchemaLoaderFuzz, SplitRecordPathRebuildsDottedPaths)
{
	// For well-formed paths (names joined by '.', each optionally followed by "[]") the split rebuilds the path exactly.
	std::mt19937 rng(1234);
	for (int iter = 0; iter < 2000; ++iter)
	{
		std::string key;
		const unsigned names = 1 + rng() % 5;
		for (unsigned n = 0; n < names; ++n)
		{
			if (n) { key += "."; }
			key += std::string(1 + rng() % 6, static_cast<char>('a' + rng() % 26));
			if (rng() % 3 == 0) { key += "[]"; }
		}
		ValueList<String64> segs;
		segs.Reserve(16);
		ASSERT_TRUE(SplitRecordPath(key.c_str(), segs)) << key;
		std::string rebuilt;
		for (unsigned i = 0; i < segs.Size(); ++i)
		{
			const std::string s = segs[i].AsCStr();
			if (s == "[]") { rebuilt += s; }
			else { if (!rebuilt.empty()) { rebuilt += "."; } rebuilt += s; }
		}
		ASSERT_EQ(rebuilt, key);
	}
}

TEST(DiaValueSchemaLoaderFuzz, GoodFixturesAreFoundAndLoad)
{
	const std::filesystem::path dir = ConformanceDir();
	ASSERT_FALSE(dir.empty());
	const std::vector<std::string> good = GoodFixtures(dir);
	ASSERT_GE(good.size(), 6u);
	for (const std::string& name : good)
	{
		std::string text;
		ASSERT_TRUE(VsTest::ReadFileBytes(dir / name, text)) << name;
		const LoadResult r = ValueSchemaLoader::LoadFromText(text.c_str(), kFile);
		EXPECT_TRUE(r.mOk) << name;
		CheckInvariants(r, name);
	}
}

TEST(DiaValueSchemaLoaderFuzz, TwoThousandSeededMutationsPerGoodFixtureNeverCrashAndKeepInvariants)
{
	const std::filesystem::path dir = ConformanceDir();
	ASSERT_FALSE(dir.empty());
	const std::vector<std::string> good = GoodFixtures(dir);
	ASSERT_GE(good.size(), 6u);

	std::mt19937 rng(1234);
	unsigned accepted = 0, rejected = 0;
	for (const std::string& name : good)
	{
		std::string text;
		ASSERT_TRUE(VsTest::ReadFileBytes(dir / name, text)) << name;
		for (int i = 0; i < 2000; ++i)
		{
			const std::string mutated = Mutate(text, rng);
			const LoadResult r = ValueSchemaLoader::LoadFromText(mutated.c_str(), kFile);
			(r.mOk ? accepted : rejected)++;
			CheckInvariants(r, name + " #" + std::to_string(i));
			if (::testing::Test::HasFatalFailure()) { return; }
		}
	}
	EXPECT_GT(rejected, 1000u) << "mutations should mostly break the files";
	EXPECT_GT(accepted, 0u) << "some mutations (whitespace, string content) stay valid";
}

TEST(DiaValueSchemaLoaderFuzz, TruncationAtEveryByteOfSmallFixtureNeverCrashes)
{
	const std::filesystem::path dir = ConformanceDir();
	ASSERT_FALSE(dir.empty());
	std::string text;
	ASSERT_TRUE(VsTest::ReadFileBytes(dir / "good-event.schema.json", text));
	for (size_t n = 0; n < text.size(); ++n)
	{
		const LoadResult r = ValueSchemaLoader::LoadFromText(text.substr(0, n).c_str(), kFile);
		CheckInvariants(r, "truncated at " + std::to_string(n));
		if (::testing::Test::HasFatalFailure()) { return; }
	}
}

TEST(DiaValueSchemaLoaderFuzz, LoadingTwiceGivesIdenticalResultsIncludingWarnings)
{
	const std::filesystem::path dir = ConformanceDir();
	ASSERT_FALSE(dir.empty());
	for (const std::string& name : GoodFixtures(dir))
	{
		std::string text;
		ASSERT_TRUE(VsTest::ReadFileBytes(dir / name, text));
		const std::string first = Dump(ValueSchemaLoader::LoadFromText(text.c_str(), kFile));
		const std::string second = Dump(ValueSchemaLoader::LoadFromText(text.c_str(), kFile));
		EXPECT_EQ(first, second) << name;
	}
}

TEST(DiaValueSchemaLoaderFuzz, DeterministicAcrossMutatedInputsToo)
{
	const std::filesystem::path dir = ConformanceDir();
	ASSERT_FALSE(dir.empty());
	std::string text;
	ASSERT_TRUE(VsTest::ReadFileBytes(dir / "good-open-blocks.schema.json", text));
	std::mt19937 rng(1234);
	for (int i = 0; i < 300; ++i)
	{
		const std::string mutated = Mutate(text, rng);
		EXPECT_EQ(Dump(ValueSchemaLoader::LoadFromText(mutated.c_str(), kFile)), Dump(ValueSchemaLoader::LoadFromText(mutated.c_str(), kFile)));
	}
}

TEST(DiaValueSchemaLoaderFuzz, GoldenDumpOfGoodParameterFile)
{
	const std::filesystem::path dir = ConformanceDir();
	ASSERT_FALSE(dir.empty());
	std::string text;
	ASSERT_TRUE(VsTest::ReadFileBytes(dir / "good-parameter-file.schema.json", text));
	const std::string actual = Dump(ValueSchemaLoader::LoadFromText(text.c_str(), "good-parameter-file.schema.json"));
	const std::string expected =
		"ok=1 for=conf.params entries=3 warnings=0\n"
		"[0] key=growth_rate for=conf.params kind=0 type=0 label=Growth rate desc=How fast a plant grows. hint=Higher is faster. unit=m/day min=0 max=10 default=1.5/ affects=biomass related=decay_rate source=conf.params.json unique=0 required=1 ref= keyRef= valueType=0 file=good-parameter-file.schema.json segs=growth_rate ui={\"colour\":\"#00ff00\",\"decimals\":2,\"delta_ramp\":\"diverging\",\"format\":\"%.2f\",\"glyph\":\"g\",\"group\":\"Growth\",\"hidden\":false,\"hotkey\":\"G\",\"layer\":\"growth\",\"ramp\":\"viridis\",\"step\":0.10000000000000001} history={\"group\":\"growth\",\"track\":true}\n"
		"[1] key=max_age for=conf.params kind=0 type=1 label=Max age desc=Lifespan in days. hint= unit= min=1 max=1000 default=100/ affects= related= source= unique=0 required=0 ref= keyRef= valueType=0 file=good-parameter-file.schema.json segs=max_age ui=null history=null\n"
		"[2] key=enabled for=conf.params kind=0 type=2 label=Enabled desc=Master switch. hint= unit= min=- max=- default=1/ affects= related= source= unique=0 required=0 ref= keyRef= valueType=0 file=good-parameter-file.schema.json segs=enabled ui=null history=null\n";
	EXPECT_EQ(actual, expected);
}
