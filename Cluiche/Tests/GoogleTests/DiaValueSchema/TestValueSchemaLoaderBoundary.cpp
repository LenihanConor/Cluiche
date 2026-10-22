#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/CategoricalColours.h>
#include "ValueSchemaTestSupport.h"
#include <cmath>
#include <functional>
#include <string>

// Loader boundary tests: every String field at exactly capacity (loads) and capacity + 1 (fails naming the field
// and the entry), file-name limits, odd documents, escapes, numbers, versions, nesting, key forms, SplitRecordPath.
//
// PINs (documented current behaviour, see the test names): enum `values: []` accepted; enum value without `id`
// accepted; empty `key` / empty `for` accepted; `default` of null/array/object silently ignored; `min > max` not
// validated; comments in the JSON are accepted (jsoncpp default) although Python's json rejects them; an embedded
// \u0000 truncates the string at the NUL; duplicate JSON keys: last wins.

using namespace Dia::ValueSchema;
using namespace Dia::Core::Containers;
using VsTest::GoodEntry;
using VsTest::GoodDoc;
using VsTest::Text;
using VsTest::Rep;
using VsTest::Contains;

namespace
{
	const char* kFile = "boundary.diavalues";

	LoadResult LoadDoc(const Json::Value& doc, const char* file = kFile) { return ValueSchemaLoader::LoadFromText(Text(doc).c_str(), file); }
	LoadResult LoadRaw(const std::string& text, const char* file = kFile) { return ValueSchemaLoader::LoadFromText(text.c_str(), file); }

	// A dotted key of exactly n characters, segments of 10 chars (<= 63 each, < 16 segments up to n = 170).
	std::string DottedOfLen(size_t n)
	{
		std::string s;
		while (s.size() < n) { s += (s.size() % 11 == 10) ? '.' : 'a'; }
		if (!s.empty() && s.back() == '.') { s.back() = 'a'; }
		return s;
	}

	using Make = std::function<Json::Value(const std::string&)>;
	using Get = std::function<std::string(const LoadResult&)>;

	struct Case
	{
		const char*	mName;
		unsigned	mCapacity;
		const char*	mField;			// field named in the failure reason
		const char*	mFailedEntry;	// entry named in the failure
		Make		mMake;
		Get			mGet;			// reads the stored value from the loaded result
		bool		mDotted = false;	// generate a dotted key instead of a run of 'a'
	};

	Json::Value WithEntry(const std::function<void(Json::Value&, const std::string&)>& mutate, const std::string& s)
	{
		Json::Value e = GoodEntry();
		mutate(e, s);
		return GoodDoc(e);
	}

	Make EntryMake(std::function<void(Json::Value&, const std::string&)> mutate)
	{
		return [mutate](const std::string& s) { return WithEntry(mutate, s); };
	}

	Json::Value EnumDoc(const char* member, const std::string& s)
	{
		Json::Value e = GoodEntry();
		e["type"] = "enum";
		Json::Value v(Json::objectValue);
		v[member] = s;
		e["values"] = Json::Value(Json::arrayValue);
		e["values"].append(v);
		return GoodDoc(e);
	}

	Json::Value PayloadDoc(const char* member, const std::string& s)
	{
		Json::Value e = GoodEntry();
		e["kind"] = "event";
		Json::Value p(Json::objectValue);
		p["name"] = "n";
		p["type"] = "float";
		p["description"] = "d";
		p[member] = s;
		e["payload"] = Json::Value(Json::arrayValue);
		e["payload"].append(p);
		return GoodDoc(e);
	}

	Json::Value VocabDoc(const char* member, const std::string& s)
	{
		Json::Value e = GoodEntry();
		e["kind"] = "event";
		Json::Value p(Json::objectValue);
		p["id"] = "i";
		p["description"] = "d";
		p[member] = s;
		e["vocabulary"] = Json::Value(Json::arrayValue);
		e["vocabulary"].append(p);
		return GoodDoc(e);
	}

	std::vector<Case> AllCases()
	{
		std::vector<Case> c;
		c.push_back({ "for", 63, "for", "", [](const std::string& s) { return GoodDoc(GoodEntry(), s.c_str()); },
			[](const LoadResult& r) { return std::string(r.mFor.AsCStr()); } });
		c.push_back({ "key", 127, "key", "<key too long>", EntryMake([](Json::Value& e, const std::string& s) { e["key"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mKey.AsCStr()); }, true });
		c.push_back({ "label", 127, "label", "k", EntryMake([](Json::Value& e, const std::string& s) { e["label"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mLabel.AsCStr()); } });
		c.push_back({ "description", 1023, "description", "k", EntryMake([](Json::Value& e, const std::string& s) { e["description"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mDescription.AsCStr()); } });
		c.push_back({ "hint", 511, "hint", "k", EntryMake([](Json::Value& e, const std::string& s) { e["hint"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mHint.AsCStr()); } });
		c.push_back({ "unit", 31, "unit", "k", EntryMake([](Json::Value& e, const std::string& s) { e["unit"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mUnit.AsCStr()); } });
		c.push_back({ "source", 127, "source", "k", EntryMake([](Json::Value& e, const std::string& s) { e["source"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mSource.AsCStr()); } });
		c.push_back({ "defaultString", 127, "default", "k", EntryMake([](Json::Value& e, const std::string& s) { e["type"] = "string"; e["default"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mDefaultString.AsCStr()); } });
		c.push_back({ "ref", 127, "ref", "k", EntryMake([](Json::Value& e, const std::string& s) { e["type"] = "ref"; e["ref"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mRef.AsCStr()); } });
		c.push_back({ "keyRef", 127, "keyRef", "k", EntryMake([](Json::Value& e, const std::string& s) { e["type"] = "map"; e["valueType"] = "float"; e["keyRef"] = s; }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mMapKeyRef.AsCStr()); } });
		c.push_back({ "affects", 127, "affects", "k", EntryMake([](Json::Value& e, const std::string& s) { e["affects"] = Json::Value(Json::arrayValue); e["affects"].append(s); }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mAffects[0].AsCStr()); } });
		c.push_back({ "related", 127, "related", "k", EntryMake([](Json::Value& e, const std::string& s) { e["related"] = Json::Value(Json::arrayValue); e["related"].append(s); }),
			[](const LoadResult& r) { return std::string(r.mEntries[0].mRelated[0].AsCStr()); } });
		c.push_back({ "enumId", 63, "id", "k", [](const std::string& s) { return EnumDoc("id", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mEnumValues[0].mId.AsCStr()); } });
		c.push_back({ "enumLabel", 127, "label", "k", [](const std::string& s) { return EnumDoc("label", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mEnumValues[0].mLabel.AsCStr()); } });
		c.push_back({ "enumDescription", 511, "description", "k", [](const std::string& s) { return EnumDoc("description", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mEnumValues[0].mDescription.AsCStr()); } });
		c.push_back({ "enumColour", 31, "colour", "k", [](const std::string& s) { return EnumDoc("colour", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mEnumValues[0].mColour.AsCStr()); } });
		c.push_back({ "payloadName", 63, "name", "k", [](const std::string& s) { return PayloadDoc("name", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mPayload[0].mName.AsCStr()); } });
		c.push_back({ "payloadDescription", 511, "description", "k", [](const std::string& s) { return PayloadDoc("description", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mPayload[0].mDescription.AsCStr()); } });
		c.push_back({ "payloadUnit", 31, "unit", "k", [](const std::string& s) { return PayloadDoc("unit", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mPayload[0].mUnit.AsCStr()); } });
		c.push_back({ "vocabId", 63, "id", "k", [](const std::string& s) { return VocabDoc("id", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mVocabulary[0].mId.AsCStr()); } });
		c.push_back({ "vocabLabel", 127, "label", "k", [](const std::string& s) { return VocabDoc("label", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mVocabulary[0].mLabel.AsCStr()); } });
		c.push_back({ "vocabDescription", 511, "description", "k", [](const std::string& s) { return VocabDoc("description", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mVocabulary[0].mDescription.AsCStr()); } });
		c.push_back({ "vocabUnit", 31, "unit", "k", [](const std::string& s) { return VocabDoc("unit", s); },
			[](const LoadResult& r) { return std::string(r.mEntries[0].mVocabulary[0].mUnit.AsCStr()); } });
		return c;
	}

	std::string Value(const Case& c, size_t n) { return c.mDotted ? DottedOfLen(n) : Rep('a', n); }
}

TEST(DiaValueSchemaLoaderBoundary, EveryStringFieldAtExactlyCapacityLoads)
{
	for (const Case& c : AllCases())
	{
		SCOPED_TRACE(c.mName);
		const std::string v = Value(c, c.mCapacity);
		ASSERT_EQ(v.size(), c.mCapacity);
		const LoadResult r = LoadDoc(c.mMake(v));
		ASSERT_TRUE(r.mOk) << r.mFailure.mEntry.AsCStr() << ": " << r.mFailure.mReason.AsCStr();
		EXPECT_EQ(c.mGet(r), v);
	}
}

TEST(DiaValueSchemaLoaderBoundary, EveryStringFieldOverCapacityFailsNamingFieldAndEntry)
{
	for (const Case& c : AllCases())
	{
		SCOPED_TRACE(c.mName);
		const LoadResult r = LoadDoc(c.mMake(Value(c, c.mCapacity + 1)));
		EXPECT_FALSE(r.mOk);
		EXPECT_EQ(r.mEntries.Size(), 0u);
		EXPECT_STREQ(r.mFailure.mFile.AsCStr(), kFile);
		EXPECT_STREQ(r.mFailure.mEntry.AsCStr(), c.mFailedEntry);
		const std::string field = std::string("'") + c.mField + "'";
		EXPECT_TRUE(Contains(r.mFailure.mReason.AsCStr(), field.c_str())) << r.mFailure.mReason.AsCStr();
		const std::string cap = "exceeds capacity of " + std::to_string(c.mCapacity) + " characters";
		EXPECT_TRUE(Contains(r.mFailure.mReason.AsCStr(), cap.c_str())) << r.mFailure.mReason.AsCStr();
	}
}

TEST(DiaValueSchemaLoaderBoundary, MuchLongerThanCapacityStillFailsCleanly)
{
	for (const Case& c : AllCases())
	{
		SCOPED_TRACE(c.mName);
		const LoadResult r = LoadDoc(c.mMake(Value(c, c.mCapacity * 4 + 10)));
		EXPECT_FALSE(r.mOk);
		EXPECT_EQ(r.mEntries.Size(), 0u);
	}
}

TEST(DiaValueSchemaLoaderBoundary, KeyLongerThan127NamesEntryAsKeyTooLong)
{
	Json::Value e = GoodEntry();
	e["key"] = DottedOfLen(300);
	const LoadResult r = LoadDoc(GoodDoc(e));
	EXPECT_FALSE(r.mOk);
	EXPECT_STREQ(r.mFailure.mEntry.AsCStr(), "<key too long>");
}

// ---- file name limits (suspected defect (b)) ----------------------------------------------------------------------

TEST(DiaValueSchemaLoaderBoundary, FileName255Loads)
{
	VsTest::AssertCounter asserts;
	const std::string name = Rep('f', 255);
	const LoadResult r = LoadDoc(GoodDoc(GoodEntry()), name.c_str());
	EXPECT_EQ(asserts.Count(), 0);
	ASSERT_TRUE(r.mOk);
	EXPECT_STREQ(r.mEntries[0].mSourceFile.AsCStr(), name.c_str());
}

TEST(DiaValueSchemaLoaderBoundary, FileName256And300DoNotFailAValidFile)
{
	for (size_t n : { 256u, 300u, 4096u })
	{
		SCOPED_TRACE(n);
		VsTest::AssertCounter asserts;
		const std::string name = Rep('f', n);
		const LoadResult r = LoadDoc(GoodDoc(GoodEntry()), name.c_str());
		EXPECT_EQ(asserts.Count(), 0) << "String256 assignment of an over-long name must not assert";
		ASSERT_TRUE(r.mOk) << "a valid file must load whatever its file name length";
		EXPECT_STREQ(r.mEntries[0].mSourceFile.AsCStr(), "<file name too long>");
	}
}

TEST(DiaValueSchemaLoaderBoundary, FileNameTooLongOnFailureDoesNotOverflow)
{
	for (size_t n : { 256u, 300u, 4096u })
	{
		SCOPED_TRACE(n);
		VsTest::AssertCounter asserts;
		const std::string name = Rep('f', n);
		const LoadResult r = LoadRaw("not json", name.c_str());
		EXPECT_EQ(asserts.Count(), 0);
		ASSERT_FALSE(r.mOk);
		EXPECT_STREQ(r.mFailure.mFile.AsCStr(), "<file name too long>");
		EXPECT_LT(strlen(r.mFailure.mFile.AsCStr()), 256u);
	}
}

TEST(DiaValueSchemaLoaderBoundary, FileNameWithFormatSpecifiersIsStoredVerbatim)
{
	const LoadResult ok = LoadDoc(GoodDoc(GoodEntry()), "%s%d%n.diavalues");
	ASSERT_TRUE(ok.mOk);
	EXPECT_STREQ(ok.mEntries[0].mSourceFile.AsCStr(), "%s%d%n.diavalues");
	const LoadResult bad = LoadRaw("{", "%s%n.diavalues");
	ASSERT_FALSE(bad.mOk);
	EXPECT_STREQ(bad.mFailure.mFile.AsCStr(), "%s%n.diavalues");
}

TEST(DiaValueSchemaLoaderBoundary, NullFileNameAndNullTextAreHandled)
{
	// Hardening: previously a null text crashed in std::string(nullptr) and a null name in strlen / AssignString.
	const LoadResult nullText = ValueSchemaLoader::LoadFromText(nullptr, kFile);
	EXPECT_FALSE(nullText.mOk);
	EXPECT_EQ(nullText.mEntries.Size(), 0u);
	const LoadResult nullName = ValueSchemaLoader::LoadFromText(Text(GoodDoc(GoodEntry())).c_str(), nullptr);
	EXPECT_TRUE(nullName.mOk);
	const LoadResult both = ValueSchemaLoader::LoadFromText(nullptr, nullptr);
	EXPECT_FALSE(both.mOk);
}

// ---- failure message bounds (suspected defect (c)) ----------------------------------------------------------------

TEST(DiaValueSchemaLoaderBoundary, FiveKilobyteGarbageGivesBoundedReason)
{
	VsTest::AssertCounter asserts;
	std::string garbage;
	for (int i = 0; i < 5000; ++i) { garbage += static_cast<char>('!' + (i * 7) % 90); }
	const LoadResult r = LoadRaw(garbage);
	EXPECT_EQ(asserts.Count(), 0);
	EXPECT_FALSE(r.mOk);
	EXPECT_LT(strlen(r.mFailure.mReason.AsCStr()), 512u);
	EXPECT_TRUE(Contains(r.mFailure.mReason.AsCStr(), "unparsable JSON"));
}

TEST(DiaValueSchemaLoaderBoundary, FiveKilobyteUnknownKeyGivesBoundedReason)
{
	VsTest::AssertCounter asserts;
	Json::Value e = GoodEntry();
	e[Rep('x', 5000)] = 1;
	const LoadResult r = LoadDoc(GoodDoc(e));
	EXPECT_EQ(asserts.Count(), 0);
	EXPECT_FALSE(r.mOk);
	const size_t len = strlen(r.mFailure.mReason.AsCStr());
	EXPECT_LE(len, 511u);
	EXPECT_TRUE(Contains(r.mFailure.mReason.AsCStr(), "unknown key"));
}

TEST(DiaValueSchemaLoaderBoundary, FiveKilobyteUnknownFileKeyGivesBoundedReason)
{
	VsTest::AssertCounter asserts;
	Json::Value doc = GoodDoc(GoodEntry());
	doc[Rep('y', 5000)] = 1;
	const LoadResult r = LoadDoc(doc);
	EXPECT_EQ(asserts.Count(), 0);
	EXPECT_FALSE(r.mOk);
	EXPECT_LE(strlen(r.mFailure.mReason.AsCStr()), 511u);
}

TEST(DiaValueSchemaLoaderBoundary, FiveKilobyteUnknownUiKeyGivesBoundedWarning)
{
	VsTest::AssertCounter asserts;
	Json::Value e = GoodEntry();
	e["ui"][Rep('z', 5000)] = 1;
	const LoadResult r = LoadDoc(GoodDoc(e));
	EXPECT_EQ(asserts.Count(), 0);
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mWarnings.Size(), 1u);
	EXPECT_LE(strlen(r.mWarnings[0].AsCStr()), 511u);
}

// ---- odd documents -------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaLoaderBoundary, NonSchemaDocumentsAreRejected)
{
	const char* docs[] = { "", "   ", "\n\t\r\n", "{}", "[]", "null", "\"x\"", "123", "true", "{\"format\":\"dia.value_schema\"}", "{", "}", "[[[[" };
	for (const char* d : docs)
	{
		SCOPED_TRACE(d);
		const LoadResult r = LoadRaw(d);
		EXPECT_FALSE(r.mOk);
		EXPECT_EQ(r.mEntries.Size(), 0u);
		EXPECT_STREQ(r.mFailure.mFile.AsCStr(), kFile);
		EXPECT_GT(strlen(r.mFailure.mReason.AsCStr()), 0u);
	}
}

TEST(DiaValueSchemaLoaderBoundary, Utf8BomIsAccepted)
{
	const std::string text = std::string("\xEF\xBB\xBF") + Text(GoodDoc(GoodEntry()));
	const LoadResult r = LoadRaw(text);
	EXPECT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_EQ(r.mEntries.Size(), 1u);
}

TEST(DiaValueSchemaLoaderBoundary, BomOnlyOrBomThenGarbageIsRejected)
{
	EXPECT_FALSE(LoadRaw("\xEF\xBB\xBF").mOk);
	EXPECT_FALSE(LoadRaw("\xEF\xBB\xBF" "garbage").mOk);
	EXPECT_FALSE(LoadRaw("\xEF\xBB").mOk);
	// A second BOM is not skipped.
	EXPECT_FALSE(LoadRaw(std::string("\xEF\xBB\xBF\xEF\xBB\xBF") + Text(GoodDoc(GoodEntry()))).mOk);
}

TEST(DiaValueSchemaLoaderBoundary, CrLfLineEndingsLoad)
{
	const std::string pretty = Json::StyledWriter().write(GoodDoc(GoodEntry()));
	std::string crlf;
	for (char c : pretty) { if (c == '\n') { crlf += "\r\n"; } else { crlf += c; } }
	const LoadResult r = LoadRaw(crlf);
	EXPECT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
}

TEST(DiaValueSchemaLoaderBoundary, PinCommentsAreAccepted)
{
	// PIN: jsoncpp's default features accept // and /* */ comments; Python's json module rejects them.
	const std::string base = Text(GoodDoc(GoodEntry()));
	const std::string text = "// leading\n/* block */" + base.substr(0, base.size() - 1) + "/* tail */";
	const LoadResult r = LoadRaw(text);
	EXPECT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
}

TEST(DiaValueSchemaLoaderBoundary, PinTrailingGarbageAfterRootIsIgnored)
{
	// PIN: jsoncpp stops after the first value; trailing text does not fail the file.
	const std::string text = Text(GoodDoc(GoodEntry())) + " trailing";
	const LoadResult r = LoadRaw(text);
	EXPECT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
}

// ---- multibyte / escapes -------------------------------------------------------------------------------------------

TEST(DiaValueSchemaLoaderBoundary, CapacityCountsBytesNotCharacters)
{
	// 2-byte chars: 63 x e-acute + 'a' = 127 bytes loads; 64 x e-acute = 128 bytes fails.
	std::string twoByteOk, twoByteBad;
	for (int i = 0; i < 63; ++i) { twoByteOk += "\xC3\xA9"; }
	twoByteOk += "a";
	for (int i = 0; i < 64; ++i) { twoByteBad += "\xC3\xA9"; }
	// 4-byte chars: 31 x U+1F600 + 3 'a' = 127 bytes; 32 x = 128 bytes.
	std::string fourByteOk, fourByteBad;
	for (int i = 0; i < 31; ++i) { fourByteOk += "\xF0\x9F\x98\x80"; }
	fourByteOk += "aaa";
	for (int i = 0; i < 32; ++i) { fourByteBad += "\xF0\x9F\x98\x80"; }
	ASSERT_EQ(twoByteOk.size(), 127u); ASSERT_EQ(twoByteBad.size(), 128u);
	ASSERT_EQ(fourByteOk.size(), 127u); ASSERT_EQ(fourByteBad.size(), 128u);

	for (const std::string* ok : { &twoByteOk, &fourByteOk })
	{
		Json::Value e = GoodEntry(); e["label"] = *ok;
		const LoadResult r = LoadDoc(GoodDoc(e));
		ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
		EXPECT_EQ(std::string(r.mEntries[0].mLabel.AsCStr()), *ok);
	}
	for (const std::string* bad : { &twoByteBad, &fourByteBad })
	{
		Json::Value e = GoodEntry(); e["label"] = *bad;
		EXPECT_FALSE(LoadDoc(GoodDoc(e)).mOk);
	}
}

TEST(DiaValueSchemaLoaderBoundary, UnicodeEscapesCountAsDecodedBytes)
{
	// Raw JSON text with é (2 UTF-8 bytes each).
	std::string label;
	for (int i = 0; i < 64; ++i) { label += "\\u00e9"; }
	const std::string head = R"({"format":"dia.value_schema","version":1,"for":"f","entries":[{"key":"k","kind":"parameter","type":"float","description":"d","label":")";
	const LoadResult bad = LoadRaw(head + label + "\"}]}");
	EXPECT_FALSE(bad.mOk);
	const LoadResult ok = LoadRaw(head + label.substr(0, 6 * 63) + "\"}]}");
	ASSERT_TRUE(ok.mOk);
	EXPECT_EQ(strlen(ok.mEntries[0].mLabel.AsCStr()), 126u);
}

TEST(DiaValueSchemaLoaderBoundary, EscapedQuoteAndBackslashRoundTrip)
{
	Json::Value e = GoodEntry("q\"k\\x");
	e["label"] = "say \"hi\" \\ bye";
	e["description"] = "d\"\\\"";
	e["type"] = "enum";
	Json::Value v(Json::objectValue);
	v["id"] = "i\"d\\";
	e["values"] = Json::Value(Json::arrayValue);
	e["values"].append(v);
	const LoadResult r = LoadDoc(GoodDoc(e));
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_STREQ(r.mEntries[0].mKey.AsCStr(), "q\"k\\x");
	EXPECT_STREQ(r.mEntries[0].mLabel.AsCStr(), "say \"hi\" \\ bye");
	EXPECT_STREQ(r.mEntries[0].mDescription.AsCStr(), "d\"\\\"");
	EXPECT_STREQ(r.mEntries[0].mEnumValues[0].mId.AsCStr(), "i\"d\\");
}

TEST(DiaValueSchemaLoaderBoundary, PinEmbeddedNulTruncatesTheValue)
{
	// PIN: the loader reads strings via asCString(), so a \u0000 inside a value ends the stored string there.
	const std::string text = R"({"format":"dia.value_schema","version":1,"for":"f","entries":[{"key":"k","kind":"parameter","type":"float","description":"d","label":"ab\u0000cd"}]})";
	const LoadResult r = LoadRaw(text);
	ASSERT_TRUE(r.mOk);
	EXPECT_STREQ(r.mEntries[0].mLabel.AsCStr(), "ab");
}

TEST(DiaValueSchemaLoaderBoundary, FormatSpecifiersInTextAreInert)
{
	VsTest::AssertCounter asserts;
	Json::Value e = GoodEntry("%s%n");
	e["description"] = "%s %d %n %x %p %%";
	const LoadResult ok = LoadDoc(GoodDoc(e));
	ASSERT_TRUE(ok.mOk);
	EXPECT_STREQ(ok.mEntries[0].mDescription.AsCStr(), "%s %d %n %x %p %%");
	EXPECT_STREQ(ok.mEntries[0].mKey.AsCStr(), "%s%n");

	// Failing fields echo the text verbatim.
	Json::Value bad = GoodEntry("%s%n");
	bad["%s %d %n"] = 1;
	const LoadResult r1 = LoadDoc(GoodDoc(bad));
	ASSERT_FALSE(r1.mOk);
	EXPECT_STREQ(r1.mFailure.mEntry.AsCStr(), "%s%n");
	EXPECT_TRUE(Contains(r1.mFailure.mReason.AsCStr(), "%s %d %n"));

	Json::Value badLabel = GoodEntry("%n%n");
	badLabel["label"] = 5;
	const LoadResult r2 = LoadDoc(GoodDoc(badLabel));
	ASSERT_FALSE(r2.mOk);
	EXPECT_STREQ(r2.mFailure.mEntry.AsCStr(), "%n%n");

	Json::Value ui = GoodEntry();
	ui["ui"]["%s%d%n"] = 1;
	const LoadResult r3 = LoadDoc(GoodDoc(ui));
	ASSERT_TRUE(r3.mOk);
	ASSERT_EQ(r3.mWarnings.Size(), 1u);
	EXPECT_TRUE(Contains(r3.mWarnings[0].AsCStr(), "'%s%d%n'"));

	Json::Value forDoc = GoodDoc(GoodEntry(), "%s%n");
	forDoc["zzz"] = 1;
	const LoadResult r4 = LoadDoc(forDoc);
	EXPECT_FALSE(r4.mOk);
	EXPECT_EQ(asserts.Count(), 0);
}

TEST(DiaValueSchemaLoaderBoundary, PinDuplicateJsonKeysLastWins)
{
	const std::string text = R"({"format":"dia.value_schema","version":1,"for":"first","for":"second","entries":[{"key":"k","kind":"parameter","type":"float","description":"d","label":"one","label":"two"}]})";
	const LoadResult r = LoadRaw(text);
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_STREQ(r.mFor.AsCStr(), "second");
	EXPECT_STREQ(r.mEntries[0].mLabel.AsCStr(), "two");
}

// ---- numbers -------------------------------------------------------------------------------------------------------

namespace
{
	LoadResult LoadWithNumberText(const char* minText, const char* maxText)
	{
		std::string text = R"({"format":"dia.value_schema","version":1,"for":"f","entries":[{"key":"k","kind":"parameter","type":"float","description":"d","label":"l")";
		if (minText) { text += std::string(",\"min\":") + minText; }
		if (maxText) { text += std::string(",\"max\":") + maxText; }
		text += "}]}";
		return LoadRaw(text);
	}
}

TEST(DiaValueSchemaLoaderBoundary, MinEqualsMaxLoads)
{
	const LoadResult r = LoadWithNumberText("5", "5");
	ASSERT_TRUE(r.mOk);
	EXPECT_TRUE(r.mEntries[0].mHasMin); EXPECT_TRUE(r.mEntries[0].mHasMax);
	EXPECT_EQ(r.mEntries[0].mMin, 5.0); EXPECT_EQ(r.mEntries[0].mMax, 5.0);
}

TEST(DiaValueSchemaLoaderBoundary, PinMinGreaterThanMaxIsNotValidated)
{
	const LoadResult r = LoadWithNumberText("10", "1");
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mEntries[0].mMin, 10.0);
	EXPECT_EQ(r.mEntries[0].mMax, 1.0);
}

TEST(DiaValueSchemaLoaderBoundary, HugeAndTinyDoublesArePreserved)
{
	const LoadResult huge = LoadWithNumberText("-1.7976931348623157e308", "1.7976931348623157e308");
	ASSERT_TRUE(huge.mOk);
	EXPECT_EQ(huge.mEntries[0].mMin, -1.7976931348623157e308);
	EXPECT_EQ(huge.mEntries[0].mMax, 1.7976931348623157e308);

	const LoadResult sub = LoadWithNumberText("1e-320", "4.9e-324");
	ASSERT_TRUE(sub.mOk);
	EXPECT_GT(sub.mEntries[0].mMin, 0.0);
	EXPECT_LT(sub.mEntries[0].mMin, 1e-300);
	EXPECT_GE(sub.mEntries[0].mMax, 0.0);
}

TEST(DiaValueSchemaLoaderBoundary, NegativeZeroAndIntVersusFloat)
{
	const LoadResult negZero = LoadWithNumberText("-0.0", "0");
	ASSERT_TRUE(negZero.mOk);
	EXPECT_EQ(negZero.mEntries[0].mMin, 0.0);
	EXPECT_TRUE(std::signbit(negZero.mEntries[0].mMin));

	const LoadResult ints = LoadWithNumberText("-3", "9007199254740993");
	ASSERT_TRUE(ints.mOk);
	EXPECT_EQ(ints.mEntries[0].mMin, -3.0);
	EXPECT_EQ(ints.mEntries[0].mMax, 9007199254740992.0);	// nearest double to 2^53 + 1

	const LoadResult floats = LoadWithNumberText("0.5", "2.50");
	ASSERT_TRUE(floats.mOk);
	EXPECT_EQ(floats.mEntries[0].mMin, 0.5);
	EXPECT_EQ(floats.mEntries[0].mMax, 2.5);
}

TEST(DiaValueSchemaLoaderBoundary, OverflowingNumberDoesNotCrash)
{
	const LoadResult r = LoadWithNumberText("1e999", "-1e999");
	if (r.mOk) { EXPECT_TRUE(std::isinf(r.mEntries[0].mMin) || r.mEntries[0].mMin != 0.0); }
	else { EXPECT_EQ(r.mEntries.Size(), 0u); }
}

TEST(DiaValueSchemaLoaderBoundary, NonNumericBoundsAreRejected)
{
	for (const char* bad : { "true", "null", "\"5\"", "[]", "{}" })
	{
		SCOPED_TRACE(bad);
		const LoadResult r = LoadWithNumberText(bad, nullptr);
		EXPECT_FALSE(r.mOk);
		EXPECT_TRUE(Contains(r.mFailure.mReason.AsCStr(), "'min' must be a number")) << r.mFailure.mReason.AsCStr();
	}
}

TEST(DiaValueSchemaLoaderBoundary, PinDefaultOfNullArrayObjectIsSilentlyIgnored)
{
	for (const char* d : { "null", "[]", "[1,2]", "{}", "{\"a\":1}" })
	{
		SCOPED_TRACE(d);
		const std::string text = std::string(R"({"format":"dia.value_schema","version":1,"for":"f","entries":[{"key":"k","kind":"parameter","type":"float","description":"d","label":"l","default":)") + d + "}]}";
		const LoadResult r = LoadRaw(text);
		ASSERT_TRUE(r.mOk);
		EXPECT_FALSE(r.mEntries[0].mHasDefault);
		EXPECT_EQ(r.mEntries[0].mDefaultNumber, 0.0);
		EXPECT_STREQ(r.mEntries[0].mDefaultString.AsCStr(), "");
	}
}

// ---- version -------------------------------------------------------------------------------------------------------

namespace
{
	LoadResult LoadWithVersionText(const char* versionText)
	{
		const std::string text = std::string(R"({"format":"dia.value_schema","version":)") + versionText + R"(,"for":"f","entries":[]})";
		return LoadRaw(text);
	}
}

TEST(DiaValueSchemaLoaderBoundary, UnrepresentableVersionNumberIsRejectedAsUnparsable)
{
	EXPECT_FALSE(LoadWithVersionText("1e400").mOk);
}

TEST(DiaValueSchemaLoaderBoundary, VersionOneLoadsAndPinFloatOneAlsoLoads)
{
	EXPECT_TRUE(LoadWithVersionText("1").mOk);
	// PIN: jsoncpp's isInt() is true for the integral double 1.0, so "version": 1.0 is accepted.
	EXPECT_TRUE(LoadWithVersionText("1.0").mOk);
}

TEST(DiaValueSchemaLoaderBoundary, OtherVersionsAreRejected)
{
	for (const char* v : { "1.5", "\"1\"", "2", "0", "-1", "4294967297", "4294967296", "true", "null", "[]", "{}", "0.999999" })
	{
		SCOPED_TRACE(v);
		const LoadResult r = LoadWithVersionText(v);
		EXPECT_FALSE(r.mOk);
		EXPECT_TRUE(Contains(r.mFailure.mReason.AsCStr(), "'version' must be the integer 1")) << r.mFailure.mReason.AsCStr();
	}
}

// ---- entry counts / nesting / width --------------------------------------------------------------------------------

TEST(DiaValueSchemaLoaderBoundary, ZeroOneAndFiveThousandEntries)
{
	for (unsigned n : { 0u, 1u, 5000u })
	{
		SCOPED_TRACE(n);
		Json::Value doc = GoodDoc(GoodEntry());
		doc["entries"] = Json::Value(Json::arrayValue);
		for (unsigned i = 0; i < n; ++i)
		{
			char key[32]; snprintf(key, sizeof(key), "k%u", i);
			doc["entries"].append(GoodEntry(key));
		}
		const LoadResult r = LoadDoc(doc);
		ASSERT_TRUE(r.mOk);
		ASSERT_EQ(r.mEntries.Size(), n);
		if (n > 0)
		{
			char last[32]; snprintf(last, sizeof(last), "k%u", n - 1);
			EXPECT_STREQ(r.mEntries[n - 1].mKey.AsCStr(), last);
			EXPECT_STREQ(r.mEntries[n - 1].mFullKey.AsCStr(), "");	// the registry fills mFullKey, not the loader
		}
	}
}

namespace
{
	std::string DeepUiDoc(unsigned depth)
	{
		std::string text = R"({"format":"dia.value_schema","version":1,"for":"f","entries":[{"key":"k","kind":"parameter","type":"float","description":"d","label":"l","ui":)";
		for (unsigned i = 0; i < depth; ++i) { text += "{\"x\":"; }
		text += "1";
		for (unsigned i = 0; i < depth; ++i) { text += "}"; }
		text += "}]}";
		return text;
	}
}

TEST(DiaValueSchemaLoaderBoundary, ModeratelyDeepUiLoads)
{
	const LoadResult r = LoadRaw(DeepUiDoc(100));
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_TRUE(r.mEntries[0].mUi.isObject());
	// jsoncpp's Reader allows 1000 nested values in total (stackLimit_g), so 900 inside the ui block still fits.
	EXPECT_TRUE(LoadRaw(DeepUiDoc(900)).mOk);
}

TEST(DiaValueSchemaLoaderBoundary, ExcessiveUiNestingIsRejectedCleanly)
{
	// jsoncpp throws Json::RuntimeError("Exceeded stackLimit in readValue().") at depth 1000; the loader must
	// convert that into an ordinary unparsable-JSON failure.
	for (unsigned depth : { 1000u, 5000u, 100000u })
	{
		SCOPED_TRACE(depth);
		const LoadResult r = LoadRaw(DeepUiDoc(depth));
		EXPECT_FALSE(r.mOk);
		EXPECT_EQ(r.mEntries.Size(), 0u);
		EXPECT_TRUE(Contains(r.mFailure.mReason.AsCStr(), "unparsable JSON")) << r.mFailure.mReason.AsCStr();
		EXPECT_STREQ(r.mFailure.mFile.AsCStr(), kFile);
	}
	// And the loader still works afterwards (jsoncpp keeps a global depth counter that parse() resets).
	EXPECT_TRUE(LoadRaw(DeepUiDoc(10)).mOk);
}

TEST(DiaValueSchemaLoaderBoundary, ExcessiveArrayNestingIsRejectedCleanly)
{
	const std::string text = std::string(100000, '[') + std::string(100000, ']');
	const LoadResult r = LoadRaw(text);
	EXPECT_FALSE(r.mOk);
}

TEST(DiaValueSchemaLoaderBoundary, VeryWideUiKeepsEverythingAndWarnsPerKey)
{
	Json::Value e = GoodEntry();
	for (int i = 0; i < 10000; ++i)
	{
		char k[32]; snprintf(k, sizeof(k), "wide%d", i);
		e["ui"][k] = i;
	}
	const LoadResult r = LoadDoc(GoodDoc(e));
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mEntries[0].mUi.size(), 10000u);
	EXPECT_EQ(r.mWarnings.Size(), 10000u);
}

TEST(DiaValueSchemaLoaderBoundary, MoreThanSixteenUnknownKeysAcrossBlocksAllWarn)
{
	Json::Value e = GoodEntry();
	for (int i = 0; i < 10; ++i)
	{
		char a[16], b[16]; snprintf(a, sizeof(a), "u%d", i); snprintf(b, sizeof(b), "h%d", i);
		e["ui"][a] = i;
		e["history"][b] = i;
	}
	e["ui"]["hidden"] = true;			// known key: no warning
	e["history"]["track"] = true;		// known key: no warning
	const LoadResult r = LoadDoc(GoodDoc(e));
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mWarnings.Size(), 20u);
	EXPECT_TRUE(Contains(r.mWarnings[0].AsCStr(), "unknown ui key"));
}

TEST(DiaValueSchemaLoaderBoundary, FailureAfterEntriesWithOpenBlocksIsClean)
{
	// Entries holding Json payloads are discarded when a later entry fails; this must not corrupt the heap.
	Json::Value doc = GoodDoc(GoodEntry("a"));
	for (int i = 0; i < 20; ++i)
	{
		Json::Value e = GoodEntry("with.ui");
		e["ui"]["group"] = "g";
		e["ui"]["extra"]["nested"] = Json::Value(Json::arrayValue);
		e["history"]["track"] = true;
		char key[16]; snprintf(key, sizeof(key), "e%d", i);
		e["key"] = key;
		doc["entries"].append(e);
	}
	Json::Value bad = GoodEntry("bad");
	bad["label"] = 5;
	doc["entries"].append(bad);
	for (int round = 0; round < 3; ++round)
	{
		LoadResult r = LoadDoc(doc);
		EXPECT_FALSE(r.mOk);
		EXPECT_EQ(r.mEntries.Size(), 0u);
		LoadResult copy = r;
		EXPECT_FALSE(copy.mOk);
	}
}

// ---- enum values ---------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaLoaderBoundary, EnumValueCountsWithMixedGivenAndAssignedColours)
{
	for (unsigned n : { 1u, 12u, 13u, 100u })
	{
		SCOPED_TRACE(n);
		Json::Value e = GoodEntry();
		e["type"] = "enum";
		e["values"] = Json::Value(Json::arrayValue);
		for (unsigned i = 0; i < n; ++i)
		{
			Json::Value v(Json::objectValue);
			char id[16]; snprintf(id, sizeof(id), "v%u", i);
			v["id"] = id;
			if (i % 3 == 0) { v["colour"] = "#010203"; }
			e["values"].append(v);
		}
		const LoadResult r = LoadDoc(GoodDoc(e));
		ASSERT_TRUE(r.mOk);
		ASSERT_EQ(r.mEntries[0].mEnumValues.Size(), n);
		for (unsigned i = 0; i < n; ++i)
		{
			const EnumValue& v = r.mEntries[0].mEnumValues[i];
			if (i % 3 == 0)
			{
				EXPECT_TRUE(v.mHasColour);
				EXPECT_STREQ(v.mColour.AsCStr(), "#010203");
			}
			else
			{
				EXPECT_FALSE(v.mHasColour);
				EXPECT_STREQ(v.mColour.AsCStr(), GetCategoricalColour(i)) << i;
			}
		}
	}
}

TEST(DiaValueSchemaLoaderBoundary, PinEnumWithEmptyValuesArrayIsAccepted)
{
	Json::Value e = GoodEntry();
	e["type"] = "enum";
	e["values"] = Json::Value(Json::arrayValue);
	const LoadResult r = LoadDoc(GoodDoc(e));
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mEntries[0].mEnumValues.Size(), 0u);
}

TEST(DiaValueSchemaLoaderBoundary, PinEnumValueWithoutIdIsAccepted)
{
	Json::Value e = GoodEntry();
	e["type"] = "enum";
	e["values"] = Json::Value(Json::arrayValue);
	e["values"].append(Json::Value(Json::objectValue));
	const LoadResult r = LoadDoc(GoodDoc(e));
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mEntries[0].mEnumValues.Size(), 1u);
	EXPECT_STREQ(r.mEntries[0].mEnumValues[0].mId.AsCStr(), "");
}

TEST(DiaValueSchemaLoaderBoundary, EnumValueMustBeObjectAndFieldsStrings)
{
	for (const char* bad : { "1", "\"x\"", "null", "[]" })
	{
		SCOPED_TRACE(bad);
		const std::string text = std::string(R"({"format":"dia.value_schema","version":1,"for":"f","entries":[{"key":"k","kind":"parameter","type":"enum","description":"d","label":"l","values":[)") + bad + "]}]}";
		EXPECT_FALSE(LoadRaw(text).mOk);
	}
	Json::Value e = GoodEntry();
	e["type"] = "enum";
	Json::Value v(Json::objectValue); v["id"] = 7;
	e["values"] = Json::Value(Json::arrayValue); e["values"].append(v);
	EXPECT_FALSE(LoadDoc(GoodDoc(e)).mOk);
}

// ---- payload / vocabulary ------------------------------------------------------------------------------------------

TEST(DiaValueSchemaLoaderBoundary, PayloadAndVocabularyEmptyAndFifty)
{
	for (unsigned n : { 0u, 1u, 50u })
	{
		SCOPED_TRACE(n);
		Json::Value e = GoodEntry();
		e["kind"] = "event";
		e["payload"] = Json::Value(Json::arrayValue);
		e["vocabulary"] = Json::Value(Json::arrayValue);
		for (unsigned i = 0; i < n; ++i)
		{
			char name[16]; snprintf(name, sizeof(name), "p%u", i);
			Json::Value p(Json::objectValue);
			p["name"] = name; p["type"] = "int"; p["description"] = "d";
			e["payload"].append(p);
			Json::Value v(Json::objectValue);
			v["id"] = name; v["description"] = "d";
			e["vocabulary"].append(v);
		}
		const LoadResult r = LoadDoc(GoodDoc(e));
		ASSERT_TRUE(r.mOk);
		ASSERT_EQ(r.mEntries[0].mPayload.Size(), n);
		ASSERT_EQ(r.mEntries[0].mVocabulary.Size(), n);
		if (n == 50)
		{
			EXPECT_STREQ(r.mEntries[0].mPayload[49].mName.AsCStr(), "p49");
			EXPECT_EQ(r.mEntries[0].mPayload[49].mType, ValueType::Int);
			EXPECT_STREQ(r.mEntries[0].mVocabulary[49].mId.AsCStr(), "p49");
		}
	}
}

// ---- key / for forms -----------------------------------------------------------------------------------------------

namespace
{
	std::string Segments(const char* key)
	{
		Json::Value e = GoodEntry(key);
		const LoadResult r = LoadDoc(GoodDoc(e));
		if (!r.mOk) { return "<rejected>"; }
		std::string out;
		for (unsigned i = 0; i < r.mEntries[0].mPathSegments.Size(); ++i)
		{
			if (i) { out += "|"; }
			out += r.mEntries[0].mPathSegments[i].AsCStr();
		}
		return out;
	}
}

TEST(DiaValueSchemaLoaderBoundary, PinOddKeyFormsAreAcceptedWithTheseSegments)
{
	EXPECT_EQ(Segments("."), "");
	EXPECT_EQ(Segments("[]"), "[]");
	EXPECT_EQ(Segments("a..b"), "a|b");
	EXPECT_EQ(Segments(".a"), "a");
	EXPECT_EQ(Segments("a."), "a");
	EXPECT_EQ(Segments("a[]b"), "a|[]|b");
	EXPECT_EQ(Segments("species[].growth_rate"), "species|[]|growth_rate");
}

TEST(DiaValueSchemaLoaderBoundary, PinEmptyKeyAndEmptyForAreAccepted)
{
	const LoadResult emptyKey = LoadDoc(GoodDoc(GoodEntry("")));
	ASSERT_TRUE(emptyKey.mOk);
	EXPECT_STREQ(emptyKey.mEntries[0].mKey.AsCStr(), "");
	EXPECT_EQ(emptyKey.mEntries[0].mPathSegments.Size(), 0u);

	const LoadResult emptyFor = LoadDoc(GoodDoc(GoodEntry(), ""));
	ASSERT_TRUE(emptyFor.mOk);
	EXPECT_STREQ(emptyFor.mFor.AsCStr(), "");
	EXPECT_STREQ(emptyFor.mEntries[0].mFor.AsCStr(), "");
}

TEST(DiaValueSchemaLoaderBoundary, KeyWithTooLongSegmentOrTooManySegmentsFails)
{
	Json::Value longSeg = GoodEntry(Rep('a', 64).c_str());
	const LoadResult r1 = LoadDoc(GoodDoc(longSeg));
	EXPECT_FALSE(r1.mOk);
	EXPECT_TRUE(Contains(r1.mFailure.mReason.AsCStr(), "path segment"));
	EXPECT_TRUE(LoadDoc(GoodDoc(GoodEntry(Rep('a', 63).c_str()))).mOk);

	std::string sixteen, seventeen;
	for (int i = 0; i < 16; ++i) { sixteen += (i ? ".s" : "s"); }
	seventeen = sixteen + ".s";
	EXPECT_TRUE(LoadDoc(GoodDoc(GoodEntry(sixteen.c_str()))).mOk);
	EXPECT_FALSE(LoadDoc(GoodDoc(GoodEntry(seventeen.c_str()))).mOk);
}

// ---- SplitRecordPath -----------------------------------------------------------------------------------------------

namespace
{
	std::string Split(const char* key, bool* ok = nullptr, unsigned reserve = 16)
	{
		ValueList<String64> segs;
		if (reserve) { segs.Reserve(reserve); }
		const bool r = SplitRecordPath(key, segs);
		if (ok) { *ok = r; }
		std::string out;
		for (unsigned i = 0; i < segs.Size(); ++i)
		{
			if (i) { out += "|"; }
			out += segs[i].AsCStr();
		}
		return out;
	}
}

TEST(DiaValueSchemaSplitRecordPath, SegmentLengthBoundary)
{
	bool ok = false;
	EXPECT_EQ(Split(Rep('a', 63).c_str(), &ok), Rep('a', 63));
	EXPECT_TRUE(ok);
	Split(Rep('a', 64).c_str(), &ok);
	EXPECT_FALSE(ok);
	// A long segment later in the path fails too, keeping what was split so far.
	EXPECT_EQ(Split((std::string("ok.") + Rep('b', 64)).c_str(), &ok), "ok");
	EXPECT_FALSE(ok);
}

TEST(DiaValueSchemaSplitRecordPath, SegmentCountBoundaryAtReserveLimit)
{
	std::string sixteen, seventeen;
	for (int i = 0; i < 16; ++i) { sixteen += (i ? ".s" : "s"); }
	seventeen = sixteen + ".s";
	bool ok = false;
	Split(sixteen.c_str(), &ok);
	EXPECT_TRUE(ok);
	Split(seventeen.c_str(), &ok);
	EXPECT_FALSE(ok);

	// "[]" markers count as segments: 8 x "s[]" = 16 segments fits, one more does not.
	std::string eightBrackets, nineBrackets;
	for (int i = 0; i < 8; ++i) { eightBrackets += (i ? ".s[]" : "s[]"); }
	nineBrackets = eightBrackets + ".s[]";
	Split(eightBrackets.c_str(), &ok);
	EXPECT_TRUE(ok);
	Split(nineBrackets.c_str(), &ok);
	EXPECT_FALSE(ok);
}

TEST(DiaValueSchemaSplitRecordPath, BracketForms)
{
	EXPECT_EQ(Split("a[]b"), "a|[]|b");
	EXPECT_EQ(Split("[][]"), "[]|[]");
	EXPECT_EQ(Split("a["), "a[");
	EXPECT_EQ(Split("a]"), "a]");
	EXPECT_EQ(Split("a[x]"), "a[x]");
	EXPECT_EQ(Split("x[].y[].z"), "x|[]|y|[]|z");
	EXPECT_EQ(Split("[]"), "[]");
	EXPECT_EQ(Split("a.[].b"), "a|[]|b");
}

TEST(DiaValueSchemaSplitRecordPath, EmptyAndNullInputs)
{
	bool ok = false;
	EXPECT_EQ(Split("", &ok), "");
	EXPECT_TRUE(ok);
	EXPECT_EQ(Split("...", &ok), "");
	EXPECT_TRUE(ok);
	EXPECT_EQ(Split(nullptr, &ok), "");	// hardening: a null key is treated as empty
	EXPECT_TRUE(ok);
}

TEST(DiaValueSchemaSplitRecordPath, PinListWithoutReserveRejectsEverySegment)
{
	// PIN: the caller must Reserve(); an unreserved list has capacity 0 and the first segment is refused.
	bool ok = true;
	Split("a", &ok, 0);
	EXPECT_FALSE(ok);
	Split("", &ok, 0);
	EXPECT_TRUE(ok);
}
