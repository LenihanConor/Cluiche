#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <json/json.h>
#include <string>

// Strict core keys, open ui/history blocks, event / map / ref entries (Feature 1 task 4).

using namespace Dia::ValueSchema;

namespace
{
	const char* kFile = "strict.diavalues";

	Json::Value GoodEntry(const char* key = "k")
	{
		Json::Value e(Json::objectValue);
		e["key"] = key;
		e["kind"] = "parameter";
		e["label"] = "Label";
		e["description"] = "Desc";
		e["type"] = "float";
		return e;
	}

	Json::Value GoodDoc(const Json::Value& entry)
	{
		Json::Value root(Json::objectValue);
		root["format"] = "dia.value_schema";
		root["version"] = 1;
		root["for"] = "test.format";
		root["entries"] = Json::Value(Json::arrayValue);
		root["entries"].append(entry);
		return root;
	}

	std::string Text(const Json::Value& v)
	{
		Json::FastWriter w;
		return w.write(v);
	}

	LoadResult LoadDoc(const Json::Value& doc) { return ValueSchemaLoader::LoadFromText(Text(doc).c_str(), kFile); }
	LoadResult LoadEntry(const Json::Value& e) { return LoadDoc(GoodDoc(e)); }

	// Asserts the file was rejected; entryKey == nullptr means a file-level failure (empty entry name).
	void ExpectRejected(const LoadResult& r, const char* entryKey, const char* reasonPart)
	{
		EXPECT_FALSE(r.mOk);
		EXPECT_STREQ(r.mFailure.mFile.AsCStr(), kFile);
		if (entryKey) { EXPECT_STREQ(r.mFailure.mEntry.AsCStr(), entryKey); }
		EXPECT_NE(std::string(r.mFailure.mReason.AsCStr()).find(reasonPart), std::string::npos)
			<< "reason was: " << r.mFailure.mReason.AsCStr();
		EXPECT_EQ(r.mEntries.Size(), 0u);

		// Loader is stateless: a later good file still loads.
		LoadResult good = LoadEntry(GoodEntry("later"));
		EXPECT_TRUE(good.mOk);
		EXPECT_EQ(good.mEntries.Size(), 1u);
	}

	void RejectEntry(const Json::Value& e, const char* reasonPart)
	{
		ExpectRejected(LoadEntry(e), e.isMember("key") && e["key"].isString() ? e["key"].asCString() : "", reasonPart);
	}

	Json::Value EventEntry()
	{
		Json::Value e = GoodEntry("evt");
		e["kind"] = "event";
		e["type"] = "string";
		return e;
	}

	Json::Value MapEntry()
	{
		Json::Value e = GoodEntry("m");
		e["type"] = "map";
		e["keyRef"] = "cow.biomes/biomes[].id";
		e["valueType"] = "float";
		return e;
	}
}

// ---- AC3: one test per strict failure ----

TEST(DiaValueSchemaLoaderStrict, GoodEntryLoads)
{
	LoadResult r = LoadEntry(GoodEntry());
	EXPECT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_EQ(r.mWarnings.Size(), 0u);
}

TEST(DiaValueSchemaLoaderStrict, MissingKeyFails)
{
	Json::Value e = GoodEntry(); e.removeMember("key");
	ExpectRejected(LoadEntry(e), "", "key");
}

TEST(DiaValueSchemaLoaderStrict, MissingKindFails)
{
	Json::Value e = GoodEntry(); e.removeMember("kind");
	RejectEntry(e, "kind");
}

TEST(DiaValueSchemaLoaderStrict, MissingLabelFails)
{
	Json::Value e = GoodEntry(); e.removeMember("label");
	RejectEntry(e, "label");
}

TEST(DiaValueSchemaLoaderStrict, MissingDescriptionFails)
{
	Json::Value e = GoodEntry(); e.removeMember("description");
	RejectEntry(e, "description");
}

TEST(DiaValueSchemaLoaderStrict, MissingTypeFails)
{
	Json::Value e = GoodEntry(); e.removeMember("type");
	RejectEntry(e, "type");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeKeyFails)
{
	Json::Value e = GoodEntry(); e["key"] = 5;
	ExpectRejected(LoadEntry(e), "", "key");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeKindFails)
{
	Json::Value e = GoodEntry(); e["kind"] = 5;
	RejectEntry(e, "kind");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeLabelFails)
{
	Json::Value e = GoodEntry(); e["label"] = 5;
	RejectEntry(e, "label");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeDescriptionFails)
{
	Json::Value e = GoodEntry(); e["description"] = true;
	RejectEntry(e, "description");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeTypeFails)
{
	Json::Value e = GoodEntry(); e["type"] = 5;
	RejectEntry(e, "type");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeUnitFails)
{
	Json::Value e = GoodEntry(); e["unit"] = 5;
	RejectEntry(e, "unit");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeHintFails)
{
	Json::Value e = GoodEntry(); e["hint"] = 5;
	RejectEntry(e, "hint");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeSourceFails)
{
	Json::Value e = GoodEntry(); e["source"] = 5;
	RejectEntry(e, "source");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeMinFails)
{
	Json::Value e = GoodEntry(); e["min"] = "zero";
	RejectEntry(e, "min");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeMaxFails)
{
	Json::Value e = GoodEntry(); e["max"] = "ten";
	RejectEntry(e, "max");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeUniqueFails)
{
	Json::Value e = GoodEntry(); e["unique"] = "yes";
	RejectEntry(e, "unique");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeRequiredFails)
{
	Json::Value e = GoodEntry(); e["required"] = 1;
	RejectEntry(e, "required");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeAffectsNotArrayFails)
{
	Json::Value e = GoodEntry(); e["affects"] = "a";
	RejectEntry(e, "affects");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeRelatedElementNotStringFails)
{
	Json::Value e = GoodEntry(); e["related"] = Json::Value(Json::arrayValue); e["related"].append(3);
	RejectEntry(e, "related");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeUiNotObjectFails)
{
	Json::Value e = GoodEntry(); e["ui"] = 5;
	RejectEntry(e, "ui");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeHistoryNotObjectFails)
{
	Json::Value e = GoodEntry(); e["history"] = Json::Value(Json::arrayValue);
	RejectEntry(e, "history");
}

TEST(DiaValueSchemaLoaderStrict, UnknownKindFails)
{
	Json::Value e = GoodEntry(); e["kind"] = "gizmo";
	RejectEntry(e, "kind");
}

TEST(DiaValueSchemaLoaderStrict, UnknownTypeFails)
{
	Json::Value e = GoodEntry(); e["type"] = "quaternion";
	RejectEntry(e, "type");
}

TEST(DiaValueSchemaLoaderStrict, BadFormatFails)
{
	Json::Value d = GoodDoc(GoodEntry()); d["format"] = "something.else";
	ExpectRejected(LoadDoc(d), nullptr, "format");
}

TEST(DiaValueSchemaLoaderStrict, MissingFormatFails)
{
	Json::Value d = GoodDoc(GoodEntry()); d.removeMember("format");
	ExpectRejected(LoadDoc(d), nullptr, "format");
}

TEST(DiaValueSchemaLoaderStrict, BadVersionFails)
{
	Json::Value d = GoodDoc(GoodEntry()); d["version"] = 2;
	ExpectRejected(LoadDoc(d), nullptr, "version");
}

TEST(DiaValueSchemaLoaderStrict, NonNumericVersionFails)
{
	Json::Value d = GoodDoc(GoodEntry()); d["version"] = "1";
	ExpectRejected(LoadDoc(d), nullptr, "version");
}

TEST(DiaValueSchemaLoaderStrict, MissingForFails)
{
	Json::Value d = GoodDoc(GoodEntry()); d.removeMember("for");
	ExpectRejected(LoadDoc(d), nullptr, "for");
}

TEST(DiaValueSchemaLoaderStrict, WrongTypeForFails)
{
	Json::Value d = GoodDoc(GoodEntry()); d["for"] = 3;
	ExpectRejected(LoadDoc(d), nullptr, "for");
}

TEST(DiaValueSchemaLoaderStrict, EntriesNotArrayFails)
{
	Json::Value d = GoodDoc(GoodEntry()); d["entries"] = Json::Value(Json::objectValue);
	ExpectRejected(LoadDoc(d), nullptr, "entries");
}

TEST(DiaValueSchemaLoaderStrict, UnknownFileLevelKeyFails)
{
	Json::Value d = GoodDoc(GoodEntry()); d["extra"] = 1;
	ExpectRejected(LoadDoc(d), nullptr, "extra");
}

TEST(DiaValueSchemaLoaderStrict, UnknownTopLevelEntryKeyFails)
{
	Json::Value e = GoodEntry(); e["bogus"] = 1;
	RejectEntry(e, "bogus");
}

TEST(DiaValueSchemaLoaderStrict, EnumWithoutValuesFails)
{
	Json::Value e = GoodEntry(); e["type"] = "enum";
	RejectEntry(e, "values");
}

TEST(DiaValueSchemaLoaderStrict, EnumValuesNotArrayFails)
{
	Json::Value e = GoodEntry(); e["type"] = "enum"; e["values"] = "a";
	RejectEntry(e, "values");
}

TEST(DiaValueSchemaLoaderStrict, LaterEntryFailureRejectsWholeFile)
{
	Json::Value d = GoodDoc(GoodEntry("first"));
	Json::Value bad = GoodEntry("second"); bad.removeMember("label");
	d["entries"].append(bad);
	ExpectRejected(LoadDoc(d), "second", "label");
}

// ---- event kind (AC5) ----

TEST(DiaValueSchemaLoaderStrict, EventLoadsWithPayloadVocabularyAndUi)
{
	Json::Value e = EventEntry();
	Json::Value p(Json::objectValue);
	p["name"] = "amount"; p["type"] = "float"; p["description"] = "How much"; p["unit"] = "kg";
	e["payload"] = Json::Value(Json::arrayValue); e["payload"].append(p);
	Json::Value p2(Json::objectValue);
	p2["name"] = "who"; p2["type"] = "string"; p2["description"] = "Actor";
	e["payload"].append(p2);
	Json::Value v(Json::objectValue);
	v["id"] = "famine"; v["label"] = "Famine"; v["description"] = "Food ran out"; v["unit"] = "x";
	e["vocabulary"] = Json::Value(Json::arrayValue); e["vocabulary"].append(v);
	e["ui"] = Json::Value(Json::objectValue); e["ui"]["colour"] = "#ff0000"; e["ui"]["glyph"] = "!";

	LoadResult r = LoadEntry(e);
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	ASSERT_EQ(r.mEntries.Size(), 1u);
	const ValueInfo& i = r.mEntries[0];
	EXPECT_EQ(i.mKind, ValueKind::Event);
	ASSERT_EQ(i.mPayload.Size(), 2u);
	EXPECT_STREQ(i.mPayload[0].mName.AsCStr(), "amount");
	EXPECT_EQ(i.mPayload[0].mType, ValueType::Float);
	EXPECT_STREQ(i.mPayload[0].mDescription.AsCStr(), "How much");
	EXPECT_STREQ(i.mPayload[0].mUnit.AsCStr(), "kg");
	EXPECT_EQ(i.mPayload[1].mType, ValueType::String);
	ASSERT_EQ(i.mVocabulary.Size(), 1u);
	EXPECT_STREQ(i.mVocabulary[0].mId.AsCStr(), "famine");
	EXPECT_STREQ(i.mVocabulary[0].mLabel.AsCStr(), "Famine");
	EXPECT_STREQ(i.mVocabulary[0].mDescription.AsCStr(), "Food ran out");
	EXPECT_STREQ(i.mVocabulary[0].mUnit.AsCStr(), "x");
	EXPECT_STREQ(i.mUi["colour"].asCString(), "#ff0000");
	EXPECT_STREQ(i.mUi["glyph"].asCString(), "!");
	EXPECT_EQ(r.mWarnings.Size(), 0u);
}

TEST(DiaValueSchemaLoaderStrict, PayloadOnNonEventFails)
{
	Json::Value e = GoodEntry(); e["payload"] = Json::Value(Json::arrayValue);
	RejectEntry(e, "payload");
}

TEST(DiaValueSchemaLoaderStrict, VocabularyOnNonEventFails)
{
	Json::Value e = GoodEntry(); e["vocabulary"] = Json::Value(Json::arrayValue);
	RejectEntry(e, "vocabulary");
}

TEST(DiaValueSchemaLoaderStrict, PayloadMissingNameFails)
{
	Json::Value e = EventEntry();
	Json::Value p(Json::objectValue); p["type"] = "float"; p["description"] = "d";
	e["payload"] = Json::Value(Json::arrayValue); e["payload"].append(p);
	RejectEntry(e, "name");
}

TEST(DiaValueSchemaLoaderStrict, PayloadMissingDescriptionFails)
{
	Json::Value e = EventEntry();
	Json::Value p(Json::objectValue); p["name"] = "n"; p["type"] = "float";
	e["payload"] = Json::Value(Json::arrayValue); e["payload"].append(p);
	RejectEntry(e, "description");
}

TEST(DiaValueSchemaLoaderStrict, PayloadBadTypeFails)
{
	Json::Value e = EventEntry();
	Json::Value p(Json::objectValue); p["name"] = "n"; p["type"] = "blob"; p["description"] = "d";
	e["payload"] = Json::Value(Json::arrayValue); e["payload"].append(p);
	RejectEntry(e, "type");
}

TEST(DiaValueSchemaLoaderStrict, VocabularyMissingIdFails)
{
	Json::Value e = EventEntry();
	Json::Value v(Json::objectValue); v["label"] = "L"; v["description"] = "d";
	e["vocabulary"] = Json::Value(Json::arrayValue); e["vocabulary"].append(v);
	RejectEntry(e, "id");
}

TEST(DiaValueSchemaLoaderStrict, VocabularyMissingDescriptionFails)
{
	Json::Value e = EventEntry();
	Json::Value v(Json::objectValue); v["id"] = "x"; v["label"] = "L";
	e["vocabulary"] = Json::Value(Json::arrayValue); e["vocabulary"].append(v);
	RejectEntry(e, "description");
}

// ---- map ----

TEST(DiaValueSchemaLoaderStrict, MapEntryLoads)
{
	Json::Value e = MapEntry(); e["min"] = 0; e["max"] = 1; e["unit"] = "frac";
	LoadResult r = LoadEntry(e);
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	const ValueInfo& i = r.mEntries[0];
	EXPECT_EQ(i.mType, ValueType::Map);
	EXPECT_STREQ(i.mMapKeyRef.AsCStr(), "cow.biomes/biomes[].id");
	EXPECT_EQ(i.mMapValueType, ValueType::Float);
	EXPECT_TRUE(i.mHasMin);
	EXPECT_TRUE(i.mHasMax);
	EXPECT_STREQ(i.mUnit.AsCStr(), "frac");
}

TEST(DiaValueSchemaLoaderStrict, MapValueTypeVariantsLoad)
{
	const char* names[] = { "float", "int", "bool", "string", "enum" };
	const ValueType types[] = { ValueType::Float, ValueType::Int, ValueType::Bool, ValueType::String, ValueType::Enum };
	for (int n = 0; n < 5; ++n)
	{
		Json::Value e = MapEntry(); e["valueType"] = names[n];
		LoadResult r = LoadEntry(e);
		ASSERT_TRUE(r.mOk) << names[n] << ": " << r.mFailure.mReason.AsCStr();
		EXPECT_EQ(r.mEntries[0].mMapValueType, types[n]);
	}
}

TEST(DiaValueSchemaLoaderStrict, MapMissingKeyRefFails)
{
	Json::Value e = MapEntry(); e.removeMember("keyRef");
	RejectEntry(e, "keyRef");
}

TEST(DiaValueSchemaLoaderStrict, MapMissingValueTypeFails)
{
	Json::Value e = MapEntry(); e.removeMember("valueType");
	RejectEntry(e, "valueType");
}

TEST(DiaValueSchemaLoaderStrict, MapInvalidValueTypeFails)
{
	Json::Value e = MapEntry(); e["valueType"] = "map";
	RejectEntry(e, "valueType");
}

TEST(DiaValueSchemaLoaderStrict, MapKeyRefWrongTypeFails)
{
	Json::Value e = MapEntry(); e["keyRef"] = 4;
	RejectEntry(e, "keyRef");
}

TEST(DiaValueSchemaLoaderStrict, KeyRefOnNonMapFails)
{
	Json::Value e = GoodEntry(); e["keyRef"] = "a/b";
	RejectEntry(e, "keyRef");
}

TEST(DiaValueSchemaLoaderStrict, ValueTypeOnNonMapFails)
{
	Json::Value e = GoodEntry(); e["valueType"] = "float";
	RejectEntry(e, "valueType");
}

// ---- ref / ref[] ----

TEST(DiaValueSchemaLoaderStrict, RefMissingRefFails)
{
	Json::Value e = GoodEntry(); e["type"] = "ref";
	RejectEntry(e, "ref");
}

TEST(DiaValueSchemaLoaderStrict, RefListMissingRefFails)
{
	Json::Value e = GoodEntry(); e["type"] = "ref[]";
	RejectEntry(e, "ref");
}

TEST(DiaValueSchemaLoaderStrict, RefOnOtherTypeFails)
{
	Json::Value e = GoodEntry(); e["ref"] = "a/b";
	RejectEntry(e, "ref");
}

TEST(DiaValueSchemaLoaderStrict, RefAndRefListLoadWithRef)
{
	Json::Value a = GoodEntry("a"); a["type"] = "ref"; a["ref"] = "x/y";
	Json::Value d = GoodDoc(a);
	Json::Value b = GoodEntry("b"); b["type"] = "ref[]"; b["ref"] = "x/z";
	d["entries"].append(b);
	LoadResult r = LoadDoc(d);
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_STREQ(r.mEntries[0].mRef.AsCStr(), "x/y");
	EXPECT_STREQ(r.mEntries[1].mRef.AsCStr(), "x/z");
}

// ---- AC7: open ui / history blocks ----

TEST(DiaValueSchemaLoaderStrict, UnknownUiKeyKeptAndWarned)
{
	Json::Value e = GoodEntry();
	e["ui"] = Json::Value(Json::objectValue); e["ui"]["group"] = "G"; e["ui"]["sparkle"] = 7;
	LoadResult r = LoadEntry(e);
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_EQ(r.mEntries[0].mUi["sparkle"].asInt(), 7);
	EXPECT_STREQ(r.mEntries[0].mUi["group"].asCString(), "G");
	ASSERT_EQ(r.mWarnings.Size(), 1u);
	const std::string w = r.mWarnings[0].AsCStr();
	EXPECT_NE(w.find(kFile), std::string::npos) << w;
	EXPECT_NE(w.find("k"), std::string::npos) << w;
	EXPECT_NE(w.find("sparkle"), std::string::npos) << w;
}

TEST(DiaValueSchemaLoaderStrict, UnknownHistoryKeyKeptAndWarned)
{
	Json::Value e = GoodEntry();
	e["history"] = Json::Value(Json::objectValue); e["history"]["track"] = true; e["history"]["colour_by"] = "x";
	LoadResult r = LoadEntry(e);
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_STREQ(r.mEntries[0].mHistory["colour_by"].asCString(), "x");
	ASSERT_EQ(r.mWarnings.Size(), 1u);
	EXPECT_NE(std::string(r.mWarnings[0].AsCStr()).find("colour_by"), std::string::npos);
}

TEST(DiaValueSchemaLoaderStrict, UnknownKeyWarnedOncePerFileNotPerEntry)
{
	Json::Value a = GoodEntry("a"); a["ui"] = Json::Value(Json::objectValue); a["ui"]["sparkle"] = 1;
	Json::Value d = GoodDoc(a);
	Json::Value b = GoodEntry("b"); b["ui"] = Json::Value(Json::objectValue); b["ui"]["sparkle"] = 2;
	d["entries"].append(b);
	Json::Value c = GoodEntry("c"); c["ui"] = Json::Value(Json::objectValue); c["ui"]["glitter"] = 3;
	d["entries"].append(c);
	LoadResult r = LoadDoc(d);
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_EQ(r.mWarnings.Size(), 2u);	// sparkle once, glitter once
	EXPECT_EQ(r.mEntries[1].mUi["sparkle"].asInt(), 2);	// still kept on every entry
}

TEST(DiaValueSchemaLoaderStrict, SameKeyInUiAndHistoryWarnsSeparately)
{
	Json::Value e = GoodEntry();
	e["ui"] = Json::Value(Json::objectValue); e["ui"]["zzz"] = 1;
	e["history"] = Json::Value(Json::objectValue); e["history"]["zzz"] = 1;
	LoadResult r = LoadEntry(e);
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mWarnings.Size(), 2u);
}

TEST(DiaValueSchemaLoaderStrict, WarningsAreNotCarriedBetweenLoads)
{
	Json::Value e = GoodEntry(); e["ui"] = Json::Value(Json::objectValue); e["ui"]["sparkle"] = 1;
	EXPECT_EQ(LoadEntry(e).mWarnings.Size(), 1u);
	EXPECT_EQ(LoadEntry(e).mWarnings.Size(), 1u);
}

TEST(DiaValueSchemaLoaderStrict, AllKnownUiAndHistoryKeysDoNotWarn)
{
	Json::Value e = GoodEntry();
	e["ui"] = Json::Value(Json::objectValue);
	const char* ui[] = { "group", "step", "decimals", "ramp", "layer", "hidden", "format", "colour", "glyph", "hotkey", "delta_ramp" };
	for (const char* k : ui) { e["ui"][k] = 1; }
	e["history"] = Json::Value(Json::objectValue);
	e["history"]["track"] = true; e["history"]["group"] = "g";
	LoadResult r = LoadEntry(e);
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_EQ(r.mWarnings.Size(), 0u);
}
