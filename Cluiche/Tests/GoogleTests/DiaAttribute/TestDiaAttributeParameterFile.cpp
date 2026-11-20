// Suite: DiaAttributeParameterFile
// ParameterFile::Load (AC 8), ApplyPack (AC 9, 28) and SchemaHash (AC 26-28).

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <DiaAttribute/ParameterFile.h>
#include <DiaAttribute/Testing/AttributeTestHelpers.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>

using namespace Dia::Attribute;
using namespace Dia::Attribute::Testing;
using Dia::Core::StringCRC;

namespace
{
    // lo [0,10]=2, hi [0,10]=8 with constraint lo <= hi; plus every kind and a map.
    std::string FixtureText(const char* speedExtra = "")
    {
        return MakeSidecarText({
            SidecarFloat("a.speed", 0, 10, 5, speedExtra),
            SidecarInt("a.count", 0, 10, 3),
            SidecarBool("a.flag", false),
            SidecarEnum("a.mode", { "Slow", "Fast", "Turbo" }, "Slow"),
            SidecarFloat("lo", 0, 10, 2),
            SidecarFloat("hi", 0, 10, 8),
            SidecarMap("m.temp", 0, 1, R"({"TemperateForest":0.5,"Desert":0.8,"Tundra":0.1})")
        }, R"([{"a":"lo","op":"<=","b":"hi"}])", "test.params");
    }

    Json::Value Parse(const std::string& text)
    {
        Json::Value v;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(text, v)) << text;
        return v;
    }

    std::string HexOf(uint64_t h)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
        return buf;
    }

    // Pack text with the test.params envelope. body is members text (no leading comma), e.g. "\"a.speed\":7".
    Json::Value MakePack(const std::string& body, const std::string& basedOn = std::string("MISSING"))
    {
        std::string s = "{\"format\":\"test.params\",\"version\":1";
        if (basedOn != "MISSING") s += ",\"basedOn\":" + basedOn;
        if (!body.empty()) s += "," + body;
        return Parse(s + "}");
    }

    std::string MatchingBasedOn(const ParameterFile& pf) { return "\"" + HexOf(pf.SchemaHash()) + "\""; }

    bool LoadFile(ParameterFile& pf, const std::string& text, ParamError* errOut = nullptr)
    {
        static const FixtureMapKeySource keys;   // ParameterFile keeps this pointer for Reload, so it must outlive the file
        ParamError err;
        const bool ok = pf.Load(text.c_str(), "test.schema.json", &keys, err);
        if (errOut != nullptr) *errOut = err;
        return ok;
    }

    std::vector<float> BaseValues(const ParameterFile& pf)
    {
        std::vector<float> v;
        for (uint32_t i = 0; i < pf.Values().GetAttributeCount(); ++i) v.push_back(pf.Values().GetBaseValueByIndex(i));
        return v;
    }

    bool BitEqual(const std::vector<float>& a, const std::vector<float>& b)
    {
        return a.size() == b.size() && (a.empty() || memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
    }

    float Base(const ParameterFile& pf, const char* key) { return pf.Values().GetBaseValue(StringCRC(key)); }

    struct CountingObserver : IAttributeObserver
    {
        std::vector<StringCRC> names;
        void OnAttributeChanged(const AttributeChangedEvent& e) override { names.push_back(e.attribute_name); }
        bool Saw(const char* key) const
        {
            for (const StringCRC& n : names) if (n == StringCRC(key)) return true;
            return false;
        }
    };

    bool HasWarningKey(const ParamWarnings& w, const char* key)
    {
        for (unsigned int i = 0; i < w.items.Size(); ++i)
            if (strcmp(w.items[i].key.AsCStr(), key) == 0) return true;
        return false;
    }

    std::string Replace(std::string s, const std::string& from, const std::string& to)
    {
        const size_t p = s.find(from);
        EXPECT_NE(p, std::string::npos) << from;
        if (p != std::string::npos) s.replace(p, from.size(), to);
        return s;
    }

    uint64_t HashOfText(const std::string& text)
    {
        ParameterFile pf;
        EXPECT_TRUE(LoadFile(pf, text));
        return pf.SchemaHash();
    }
}

// ---------------------------------------------------------------------------
// Load (AC 8)
// ---------------------------------------------------------------------------
TEST(DiaAttributeParameterFileLoad, ValidSidecarGivesDefaults)
{
    ParameterFile pf;
    ParamError err;
    ASSERT_TRUE(LoadFile(pf, FixtureText(), &err)) << err.key.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_TRUE(pf.IsLoaded());
    EXPECT_EQ(pf.Schema().GetAttributeCount(), 6u + 3u);
    EXPECT_EQ(pf.Values().GetAttributeCount(), 9u);
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 5.0f);
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 3.0f);
    EXPECT_FLOAT_EQ(Base(pf, "a.flag"), 0.0f);
    EXPECT_FLOAT_EQ(Base(pf, "a.mode"), 0.0f);
    EXPECT_FLOAT_EQ(Base(pf, "lo"), 2.0f);
    EXPECT_FLOAT_EQ(Base(pf, "hi"), 8.0f);
    EXPECT_FLOAT_EQ(Base(pf, "m.temp.Desert"), 0.8f);
}

TEST(DiaAttributeParameterFileLoad, EmptySidecarLoads)
{
    ParameterFile pf;
    ASSERT_TRUE(LoadFile(pf, MakeSidecarText({}, nullptr, "test.params")));
    EXPECT_TRUE(pf.IsLoaded());
    EXPECT_EQ(pf.Values().GetAttributeCount(), 0u);

    ParamWarnings warn;
    ParamError err;
    EXPECT_TRUE(pf.ApplyPack(MakePack("", MatchingBasedOn(pf)), warn, err));
    EXPECT_EQ(warn.items.Size(), 0u);
}

static void ExpectLoadFails(const std::string& text, const char* key)
{
    ParameterFile pf;
    ParamError err;
    EXPECT_FALSE(LoadFile(pf, text, &err));
    if (key != nullptr) EXPECT_STREQ(err.key.AsCStr(), key) << err.reason.AsCStr();
    EXPECT_GT(strlen(err.key.AsCStr()), 0u);
    EXPECT_GT(strlen(err.reason.AsCStr()), 0u);
    EXPECT_FALSE(pf.IsLoaded());
    EXPECT_EQ(pf.Schema().GetAttributeCount(), 0u);
    EXPECT_EQ(pf.Values().GetAttributeCount(), 0u);
}

TEST(DiaAttributeParameterFileLoad, DefaultOutOfRangeFails)
{
    ExpectLoadFails(MakeSidecarText({ SidecarFloat("ok", 0, 1, 0.5), SidecarFloat("bad", 0, 1, 5) }), "bad");
}

TEST(DiaAttributeParameterFileLoad, DefaultWrongKindFails)
{
    const std::string intEntry = R"({"key":"cnt","kind":"parameter","label":"l","description":"d","type":"int","min":0,"max":10,"default":2.5})";
    ExpectLoadFails(MakeSidecarText({ SidecarFloat("ok", 0, 1, 0.5), intEntry }), "cnt");
}

TEST(DiaAttributeParameterFileLoad, ViolatedConstraintFails)
{
    ExpectLoadFails(MakeSidecarText({ SidecarFloat("lo", 0, 10, 9), SidecarFloat("hi", 0, 10, 2) },
                                    R"([{"a":"lo","op":"<=","b":"hi"}])"), "lo");
}

TEST(DiaAttributeParameterFileLoad, BadBandFails)
{
    ExpectLoadFails(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5, R"(,"bands":{"HIGH":[0.5,2.0]})") }), "x");
}

TEST(DiaAttributeParameterFileLoad, WrongFormatFails)
{
    const std::string text = Replace(FixtureText(), "dia.value_schema", "something.else");
    ParameterFile pf;
    ParamError err;
    EXPECT_FALSE(LoadFile(pf, text, &err));
    EXPECT_GT(strlen(err.key.AsCStr()), 0u);
    EXPECT_NE(std::string(err.reason.AsCStr()).find("format"), std::string::npos) << err.reason.AsCStr();
    EXPECT_FALSE(pf.IsLoaded());
}

TEST(DiaAttributeParameterFileLoad, WrongVersionFails)
{
    const std::string text = Replace(FixtureText(), "\"version\":1", "\"version\":2");
    ParameterFile pf;
    ParamError err;
    EXPECT_FALSE(LoadFile(pf, text, &err));
    EXPECT_GT(strlen(err.key.AsCStr()), 0u);
    EXPECT_NE(std::string(err.reason.AsCStr()).find("version"), std::string::npos) << err.reason.AsCStr();
    EXPECT_FALSE(pf.IsLoaded());
}

TEST(DiaAttributeParameterFileLoad, MapWithoutKeySourceFailsNamingKey)
{
    ParameterFile pf;
    ParamError err;
    EXPECT_FALSE(pf.Load(FixtureText().c_str(), "t", nullptr, err));
    EXPECT_STREQ(err.key.AsCStr(), "m.temp");
    EXPECT_FALSE(pf.IsLoaded());
}

TEST(DiaAttributeParameterFileLoad, LoadTwiceFailsAndKeepsFirst)
{
    ParameterFile pf;
    ASSERT_TRUE(LoadFile(pf, FixtureText()));
    ParamError err;
    EXPECT_FALSE(LoadFile(pf, FixtureText(), &err));
    EXPECT_TRUE(pf.IsLoaded());
    EXPECT_EQ(pf.Values().GetAttributeCount(), 9u);
}

// ---------------------------------------------------------------------------
// ApplyPack (AC 9)
// ---------------------------------------------------------------------------
class DiaAttributeParameterFilePack : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(LoadFile(pf, FixtureText()));
        pf.Values().GetObserverSubject().Subscribe(&obs);
    }
    void TearDown() override { pf.Values().GetObserverSubject().Unsubscribe(&obs); }

    bool Apply(const std::string& body) { return pf.ApplyPack(MakePack(body, MatchingBasedOn(pf)), warn, err); }

    void ExpectRejectedUntouched(const Json::Value& pack, const char* reasonPart)
    {
        const std::vector<float> before = BaseValues(pf);
        EXPECT_FALSE(pf.ApplyPack(pack, warn, err));
        EXPECT_TRUE(BitEqual(before, BaseValues(pf)));
        EXPECT_TRUE(obs.names.empty());
        EXPECT_NE(std::string(err.reason.AsCStr()).find(reasonPart), std::string::npos) << err.reason.AsCStr();
    }

    ParameterFile pf;
    CountingObserver obs;
    ParamWarnings warn;
    ParamError err;
};

TEST_F(DiaAttributeParameterFilePack, SparsePackChangesOnlyItsKeys)
{
    const std::vector<float> before = BaseValues(pf);
    ASSERT_TRUE(Apply(R"("a.speed":7.5)"));
    EXPECT_EQ(warn.items.Size(), 0u);
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 7.5f);
    const std::vector<float> after = BaseValues(pf);
    for (size_t i = 0; i < before.size(); ++i)
    {
        if (i == 0) continue;   // a.speed is the first schema entry
        EXPECT_EQ(memcmp(&before[i], &after[i], sizeof(float)), 0) << i;
    }
    ASSERT_EQ(obs.names.size(), 1u);
    EXPECT_TRUE(obs.Saw("a.speed"));
}

TEST_F(DiaAttributeParameterFilePack, ObserversFireOnlyForChangedValues)
{
    ASSERT_TRUE(Apply(R"("a.speed":5.0,"a.count":4)"));   // speed equals its default
    ASSERT_EQ(obs.names.size(), 1u);
    EXPECT_TRUE(obs.Saw("a.count"));
}

TEST_F(DiaAttributeParameterFilePack, ObserversSeeFullyCommittedState)
{
    struct StateObserver : IAttributeObserver
    {
        ParameterFile* f = nullptr;
        float countAtNotify = -1.0f;
        void OnAttributeChanged(const AttributeChangedEvent& e) override
        {
            if (e.attribute_name == StringCRC("a.speed")) countAtNotify = f->Values().GetBaseValue(StringCRC("a.count"));
        }
    } so;
    so.f = &pf;
    pf.Values().GetObserverSubject().Subscribe(&so);
    ASSERT_TRUE(Apply(R"("a.speed":6,"a.count":9)"));
    pf.Values().GetObserverSubject().Unsubscribe(&so);
    EXPECT_FLOAT_EQ(so.countAtNotify, 9.0f);
}

TEST_F(DiaAttributeParameterFilePack, UnknownKeyDroppedWithWarning)
{
    ASSERT_TRUE(Apply(R"("nope.key":1,"a.count":5)"));
    EXPECT_EQ(warn.items.Size(), 1u);
    EXPECT_TRUE(HasWarningKey(warn, "nope.key"));
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 5.0f);
}

TEST_F(DiaAttributeParameterFilePack, DuplicateLookingKeysAreDistinct)
{
    ASSERT_TRUE(Apply(R"("a.speed":6,"A.speed":9,"a.speed ":9)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 6.0f);
    EXPECT_EQ(warn.items.Size(), 2u);
}

TEST_F(DiaAttributeParameterFilePack, NonWholeIntDropped)
{
    ASSERT_TRUE(Apply(R"("a.count":2.5)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 3.0f);
    EXPECT_TRUE(HasWarningKey(warn, "a.count"));
    EXPECT_TRUE(obs.names.empty());
}

TEST_F(DiaAttributeParameterFilePack, WholeIntAsRealAccepted)
{
    ASSERT_TRUE(Apply(R"("a.count":6.0)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 6.0f);
    EXPECT_EQ(warn.items.Size(), 0u);
}

TEST_F(DiaAttributeParameterFilePack, OutOfRangeClampsWithWarning)
{
    ASSERT_TRUE(Apply(R"("a.speed":50,"a.count":-4)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 10.0f);
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 0.0f);
    EXPECT_TRUE(HasWarningKey(warn, "a.speed"));
    EXPECT_TRUE(HasWarningKey(warn, "a.count"));
    EXPECT_EQ(warn.items.Size(), 2u);
}

TEST_F(DiaAttributeParameterFilePack, NullValueDroppedWithWarning)
{
    ASSERT_TRUE(Apply(R"("a.speed":null)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 5.0f);
    EXPECT_TRUE(HasWarningKey(warn, "a.speed"));
}

TEST_F(DiaAttributeParameterFilePack, WrongTypedValueDropped)
{
    ASSERT_TRUE(Apply(R"("a.speed":"fast","a.count":[1],"a.flag":"yes")"));
    EXPECT_EQ(warn.items.Size(), 3u);
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 5.0f);
}

TEST_F(DiaAttributeParameterFilePack, NegativeZeroKeptBitExact)
{
    ASSERT_TRUE(Apply(R"("a.speed":-0.0)"));
    const float v = Base(pf, "a.speed");
    EXPECT_EQ(v, 0.0f);
    EXPECT_TRUE(std::signbit(v));
    EXPECT_EQ(warn.items.Size(), 0u);
}

TEST_F(DiaAttributeParameterFilePack, EnumByIdAndUnknownId)
{
    ASSERT_TRUE(Apply(R"("a.mode":"Turbo")"));
    EXPECT_FLOAT_EQ(Base(pf, "a.mode"), 2.0f);
    EXPECT_EQ(warn.items.Size(), 0u);

    ParamWarnings w2;
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("a.mode":"Warp")", MatchingBasedOn(pf)), w2, err));
    EXPECT_FLOAT_EQ(Base(pf, "a.mode"), 2.0f);
    EXPECT_TRUE(HasWarningKey(w2, "a.mode"));

    ParamWarnings w3;
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("a.mode":1)", MatchingBasedOn(pf)), w3, err));   // ordinal numbers are not ids
    EXPECT_FLOAT_EQ(Base(pf, "a.mode"), 2.0f);
    EXPECT_TRUE(HasWarningKey(w3, "a.mode"));
}

TEST_F(DiaAttributeParameterFilePack, BoolAcceptsTrueFalseZeroOneAndDropsTwo)
{
    ASSERT_TRUE(Apply(R"("a.flag":true)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.flag"), 1.0f);
    ASSERT_TRUE(Apply(R"("a.flag":false)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.flag"), 0.0f);
    ASSERT_TRUE(Apply(R"("a.flag":1)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.flag"), 1.0f);
    ASSERT_TRUE(Apply(R"("a.flag":0)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.flag"), 0.0f);
    EXPECT_EQ(warn.items.Size(), 0u);

    ParamWarnings w2;
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("a.flag":2)", MatchingBasedOn(pf)), w2, err));
    EXPECT_FLOAT_EQ(Base(pf, "a.flag"), 0.0f);
    EXPECT_TRUE(HasWarningKey(w2, "a.flag"));
}

TEST_F(DiaAttributeParameterFilePack, MapNestedObjectIsSparse)
{
    ASSERT_TRUE(Apply(R"("m.temp":{"Desert":0.9})"));
    EXPECT_FLOAT_EQ(Base(pf, "m.temp.Desert"), 0.9f);
    EXPECT_FLOAT_EQ(Base(pf, "m.temp.TemperateForest"), 0.5f);
    EXPECT_FLOAT_EQ(Base(pf, "m.temp.Tundra"), 0.1f);
    EXPECT_EQ(warn.items.Size(), 0u);
    ASSERT_EQ(obs.names.size(), 1u);
    EXPECT_TRUE(obs.Saw("m.temp.Desert"));
}

TEST_F(DiaAttributeParameterFilePack, MapUnknownKeyClampAndNonObject)
{
    ASSERT_TRUE(Apply(R"("m.temp":{"Swamp":0.2,"Tundra":1.5})"));
    EXPECT_FLOAT_EQ(Base(pf, "m.temp.Tundra"), 1.0f);
    EXPECT_TRUE(HasWarningKey(warn, "m.temp.Swamp"));
    EXPECT_TRUE(HasWarningKey(warn, "m.temp.Tundra"));
    EXPECT_EQ(warn.items.Size(), 2u);

    ParamWarnings w2;
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("m.temp":0.5)", MatchingBasedOn(pf)), w2, err));   // a map needs an object
    EXPECT_TRUE(HasWarningKey(w2, "m.temp"));
    EXPECT_FLOAT_EQ(Base(pf, "m.temp.Desert"), 0.8f);
}

TEST_F(DiaAttributeParameterFilePack, ManyDroppableKeysFillDropped)
{
    std::string body;
    for (int i = 0; i < 70; ++i)
    {
        if (i > 0) body += ",";
        body += "\"x" + std::to_string(i) + "\":1";
    }
    ASSERT_TRUE(Apply(body));
    EXPECT_EQ(warn.items.Size(), 64u);
    EXPECT_EQ(warn.dropped, 6u);
}

TEST_F(DiaAttributeParameterFilePack, WrongFormatRejected)
{
    ExpectRejectedUntouched(Parse(R"({"format":"other.params","version":1,"a.speed":9})"), "format");
    EXPECT_STREQ(err.key.AsCStr(), "format");
}

TEST_F(DiaAttributeParameterFilePack, MissingFormatRejected)
{
    ExpectRejectedUntouched(Parse(R"({"version":1,"a.speed":9})"), "format");
}

TEST_F(DiaAttributeParameterFilePack, WrongVersionRejected)
{
    ExpectRejectedUntouched(Parse(R"({"format":"test.params","version":2,"a.speed":9})"), "version");
    EXPECT_STREQ(err.key.AsCStr(), "version");
    ParamWarnings w;
    EXPECT_FALSE(pf.ApplyPack(Parse(R"({"format":"test.params","version":"1","a.speed":9})"), w, err));
}

TEST_F(DiaAttributeParameterFilePack, NonObjectPackRejected)
{
    const char* texts[] = { "[1,2]", "null", "\"pack\"", "5" };
    for (const char* t : texts)
    {
        Json::Value v;
        Json::Reader r;
        ASSERT_TRUE(r.parse(t, v));
        const std::vector<float> before = BaseValues(pf);
        EXPECT_FALSE(pf.ApplyPack(v, warn, err)) << t;
        EXPECT_TRUE(BitEqual(before, BaseValues(pf)));
        EXPECT_GT(strlen(err.reason.AsCStr()), 0u);
    }
    EXPECT_TRUE(obs.names.empty());
}

TEST_F(DiaAttributeParameterFilePack, ConstraintFailureRejectsWholePack)
{
    // lo=9 breaks lo <= hi (hi stays 8); the valid a.count in the same pack must not apply either.
    ExpectRejectedUntouched(MakePack(R"("lo":9,"a.count":7)", MatchingBasedOn(pf)), "constraint");
    EXPECT_STREQ(err.key.AsCStr(), "lo");
    EXPECT_STREQ(err.otherKey.AsCStr(), "hi");
}

TEST_F(DiaAttributeParameterFilePack, ConstraintHoldsAcrossTwoKeysInSamePack)
{
    ASSERT_TRUE(Apply(R"("lo":9,"hi":9.5)"));
    EXPECT_FLOAT_EQ(Base(pf, "lo"), 9.0f);
    EXPECT_FLOAT_EQ(Base(pf, "hi"), 9.5f);
}

TEST_F(DiaAttributeParameterFilePack, ClampedValueCheckedAgainstConstraint)
{
    // hi clamps to 10, lo clamps to 10: lo <= hi still holds.
    ASSERT_TRUE(Apply(R"("lo":99,"hi":99)"));
    EXPECT_FLOAT_EQ(Base(pf, "lo"), 10.0f);
}

TEST_F(DiaAttributeParameterFilePack, SequentialPacksLastWins)
{
    ASSERT_TRUE(Apply(R"("a.speed":6)"));
    ASSERT_TRUE(Apply(R"("a.speed":7)"));
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 7.0f);
}

// ---------------------------------------------------------------------------
// basedOn / stale packs (AC 28) and SchemaHash (AC 26, 27)
// ---------------------------------------------------------------------------
TEST_F(DiaAttributeParameterFilePack, MatchingBasedOnGivesNoWarning)
{
    ASSERT_TRUE(Apply(R"("a.count":4)"));
    EXPECT_EQ(warn.items.Size(), 0u);
}

TEST_F(DiaAttributeParameterFilePack, MismatchedBasedOnWarnsOnceAndApplies)
{
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("a.count":4)", "\"deadbeefdeadbeef\""), warn, err));
    EXPECT_EQ(warn.items.Size(), 1u);
    EXPECT_TRUE(HasWarningKey(warn, "basedOn"));
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 4.0f);
}

TEST_F(DiaAttributeParameterFilePack, MissingBasedOnWarnsOnceAndApplies)
{
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("a.count":4)"), warn, err));
    EXPECT_EQ(warn.items.Size(), 1u);
    EXPECT_TRUE(HasWarningKey(warn, "basedOn"));
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 4.0f);
}

TEST_F(DiaAttributeParameterFilePack, NonStringBasedOnIsStaleOnce)
{
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("a.count":4)", "12345"), warn, err));
    EXPECT_EQ(warn.items.Size(), 1u);
    EXPECT_TRUE(HasWarningKey(warn, "basedOn"));
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 4.0f);
}

TEST_F(DiaAttributeParameterFilePack, RejectedPackAddsNoStaleWarningRequirement)
{
    // A stale pack that also breaks a constraint is still rejected.
    EXPECT_FALSE(pf.ApplyPack(MakePack(R"("lo":9)", "\"0000000000000000\""), warn, err));
    EXPECT_FLOAT_EQ(Base(pf, "lo"), 2.0f);
}

TEST(DiaAttributeParameterFileSchemaHash, StableAcrossTwoLoads)
{
    EXPECT_EQ(HashOfText(FixtureText()), HashOfText(FixtureText()));
    EXPECT_NE(HashOfText(FixtureText()), 0u);
}

TEST(DiaAttributeParameterFileSchemaHash, PinnedLiteralForSmallFixture)
{
    const std::string text = MakeSidecarText({ SidecarFloat("a", 0, 1, 0.5), SidecarInt("b", 0, 9, 3) }, nullptr, "pin.params");
    EXPECT_EQ(HashOfText(text), 0xb327c5415f6b4df2ull);
}

TEST(DiaAttributeParameterFileSchemaHash, PresentationDoesNotChangeHash)
{
    const uint64_t base = HashOfText(FixtureText());
    EXPECT_EQ(base, HashOfText(Replace(FixtureText(), "\"label\":\"l\"", "\"label\":\"A different label\"")));
    EXPECT_EQ(base, HashOfText(Replace(FixtureText(), "\"description\":\"d\"", "\"description\":\"Other words\"")));
    EXPECT_EQ(base, HashOfText(FixtureText(R"(,"hint":"a hint","unit":"m","ui":{"group":"G"})")));
    EXPECT_EQ(base, HashOfText(Replace(FixtureText(), "\"label\":\"Fast\"", "\"label\":\"Quick\"")));
    EXPECT_EQ(base, HashOfText(Replace(FixtureText(), "\"label\":\"Slow\"", "\"label\":\"Slow\",\"colour\":\"#ff0000\"")));
}

TEST(DiaAttributeParameterFileSchemaHash, MeaningChangesChangeHash)
{
    const std::string base = FixtureText();
    const uint64_t h = HashOfText(base);
    EXPECT_NE(h, HashOfText(Replace(base, "\"key\":\"a.count\"", "\"key\":\"a.counts\"")));          // key
    EXPECT_NE(h, HashOfText(Replace(base, "\"type\":\"int\"", "\"type\":\"float\"")));               // kind
    EXPECT_NE(h, HashOfText(Replace(base, "\"max\":10.0,\"default\":5.0", "\"max\":11.0,\"default\":5.0")));   // max
    EXPECT_NE(h, HashOfText(Replace(base, "\"min\":0.0,\"max\":10.0,\"default\":5.0", "\"min\":1.0,\"max\":10.0,\"default\":5.0"))); // min
    EXPECT_NE(h, HashOfText(Replace(base, "\"default\":5.0", "\"default\":6.0")));                   // default
    EXPECT_NE(h, HashOfText(Replace(base, "\"id\":\"Fast\"", "\"id\":\"Quick\"")));                  // enum id
}

TEST(DiaAttributeParameterFileSchemaHash, EnumOrderChangesHash)
{
    const std::string a = MakeSidecarText({ SidecarEnum("e", { "One", "Two" }, "One") });
    const std::string b = MakeSidecarText({ SidecarEnum("e", { "Two", "One" }, "One") });
    EXPECT_NE(HashOfText(a), HashOfText(b));
}

TEST(DiaAttributeParameterFileSchemaHash, BandChangeChangesHash)
{
    const uint64_t a = HashOfText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5, R"(,"bands":{"LOW":[0.0,0.3]})") }));
    const uint64_t b = HashOfText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5, R"(,"bands":{"LOW":[0.0,0.4]})") }));
    const uint64_t c = HashOfText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5, R"(,"bands":{"LOWER":[0.0,0.3]})") }));
    const uint64_t d = HashOfText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5) }));
    EXPECT_NE(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(DiaAttributeParameterFileSchemaHash, ConstraintChangeChangesHash)
{
    auto make = [](const char* op, bool withConstraint) {
        return MakeSidecarText({ SidecarFloat("lo", 0, 10, 2), SidecarFloat("hi", 0, 10, 8) },
                               withConstraint ? (std::string("[{\"a\":\"lo\",\"op\":\"") + op + "\",\"b\":\"hi\"}]").c_str() : nullptr);
    };
    const uint64_t none = HashOfText(make("<", false));
    const uint64_t le = HashOfText(make("<=", true));
    const uint64_t lt = HashOfText(make("<", true));
    EXPECT_NE(none, le);
    EXPECT_NE(le, lt);
}

TEST(DiaAttributeParameterFileSchemaHash, MapKeyRefAndDefaultsChangeHash)
{
    const std::string base = FixtureText();
    const uint64_t h = HashOfText(base);
    EXPECT_NE(h, HashOfText(Replace(base, "\"Desert\":0.8", "\"Desert\":0.7")));
}

TEST(DiaAttributeParameterFileSchemaHash, NegativeZeroNormalised)
{
    const uint64_t a = HashOfText(MakeSidecarText({ SidecarFloat("x", -0.0, 1, 0.5) }));
    const uint64_t b = HashOfText(MakeSidecarText({ SidecarFloat("x", 0.0, 1, 0.5) }));
    EXPECT_EQ(a, b);
}

TEST(DiaAttributeParameterFileSchemaHash, ValuesDoNotAffectSchemaHash)
{
    ParameterFile pf;
    ASSERT_TRUE(LoadFile(pf, FixtureText()));
    const uint64_t before = pf.SchemaHash();
    ParamWarnings w;
    ParamError err;
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("a.speed":9)", MatchingBasedOn(pf)), w, err));
    EXPECT_EQ(before, pf.SchemaHash());
}

// ---------------------------------------------------------------------------
// Set (AC 10)
// ---------------------------------------------------------------------------
class DiaAttributeParameterFileSet : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(LoadFile(pf, FixtureText()));
        pf.Values().GetObserverSubject().Subscribe(&obs);
    }
    void TearDown() override { pf.Values().GetObserverSubject().Unsubscribe(&obs); }

    void ExpectRejected(const char* key, const Json::Value& v, const char* errKey, const char* errOtherKey = "")
    {
        const std::vector<float> before = BaseValues(pf);
        EXPECT_FALSE(pf.Set(StringCRC(key), v, err)) << key;
        EXPECT_TRUE(BitEqual(before, BaseValues(pf))) << key;
        EXPECT_TRUE(obs.names.empty()) << key;
        EXPECT_STREQ(err.key.AsCStr(), errKey);
        EXPECT_STREQ(err.otherKey.AsCStr(), errOtherKey);
        EXPECT_NE(std::string(err.reason.AsCStr()), "");
    }

    ParameterFile pf;
    CountingObserver obs;
    ParamError err;
};

TEST_F(DiaAttributeParameterFileSet, SuccessFiresOnceWithOldAndNew)
{
    struct EvObserver : IAttributeObserver
    {
        int calls = 0; float oldV = -1, newV = -1;
        void OnAttributeChanged(const AttributeChangedEvent& e) override { ++calls; oldV = e.old_value; newV = e.new_value; }
    } ev;
    pf.Values().GetObserverSubject().Subscribe(&ev);
    ASSERT_TRUE(pf.Set(StringCRC("a.speed"), Json::Value(7.5), err));
    pf.Values().GetObserverSubject().Unsubscribe(&ev);
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 7.5f);
    EXPECT_EQ(ev.calls, 1);
    EXPECT_FLOAT_EQ(ev.oldV, 5.0f);
    EXPECT_FLOAT_EQ(ev.newV, 7.5f);
    EXPECT_EQ(obs.names.size(), 1u);
}

TEST_F(DiaAttributeParameterFileSet, SameValueFiresNothing)
{
    ASSERT_TRUE(pf.Set(StringCRC("a.speed"), Json::Value(5.0), err));
    EXPECT_TRUE(obs.names.empty());
}

TEST_F(DiaAttributeParameterFileSet, OnlyTheKeyChanges)
{
    const std::vector<float> before = BaseValues(pf);
    ASSERT_TRUE(pf.Set(StringCRC("a.count"), Json::Value(9), err));
    const std::vector<float> after = BaseValues(pf);
    int diff = 0;
    for (size_t i = 0; i < before.size(); ++i) if (memcmp(&before[i], &after[i], sizeof(float)) != 0) ++diff;
    EXPECT_EQ(diff, 1);
    EXPECT_FLOAT_EQ(Base(pf, "a.count"), 9.0f);
}

TEST_F(DiaAttributeParameterFileSet, EnumAndBoolAccepted)
{
    ASSERT_TRUE(pf.Set(StringCRC("a.mode"), Json::Value("Turbo"), err));
    EXPECT_FLOAT_EQ(Base(pf, "a.mode"), 2.0f);
    ASSERT_TRUE(pf.Set(StringCRC("a.flag"), Json::Value(true), err));
    EXPECT_FLOAT_EQ(Base(pf, "a.flag"), 1.0f);
    ASSERT_TRUE(pf.Set(StringCRC("a.flag"), Json::Value(0), err));
    EXPECT_FLOAT_EQ(Base(pf, "a.flag"), 0.0f);
}

TEST_F(DiaAttributeParameterFileSet, MapColumn)
{
    ASSERT_TRUE(pf.Set(StringCRC("m.temp.Desert"), Json::Value(0.25), err));
    EXPECT_FLOAT_EQ(Base(pf, "m.temp.Desert"), 0.25f);
    EXPECT_FLOAT_EQ(Base(pf, "m.temp.Tundra"), 0.1f);
    ASSERT_EQ(obs.names.size(), 1u);
    EXPECT_TRUE(obs.Saw("m.temp.Desert"));
    obs.names.clear();
    ExpectRejected("m.temp.Desert", Json::Value(5.0), "m.temp.Desert");   // map range is [0,1]
}

TEST_F(DiaAttributeParameterFileSet, UnknownKeyRejected)
{
    ExpectRejected("a.nothing", Json::Value(1), "a.nothing");
    ExpectRejected("m.temp.Mars", Json::Value(0.5), "m.temp.Mars");
}

TEST_F(DiaAttributeParameterFileSet, OutOfRangeRejected)
{
    ExpectRejected("a.speed", Json::Value(11.0), "a.speed");
    ExpectRejected("a.speed", Json::Value(-0.5), "a.speed");
}

TEST_F(DiaAttributeParameterFileSet, WrongKindRejected)
{
    ExpectRejected("a.count", Json::Value(2.5), "a.count");
    ExpectRejected("a.mode", Json::Value("Warp"), "a.mode");
    ExpectRejected("a.mode", Json::Value(1), "a.mode");
    ExpectRejected("a.flag", Json::Value(2), "a.flag");
    ExpectRejected("a.speed", Json::Value("fast"), "a.speed");
    ExpectRejected("a.speed", Json::Value(Json::nullValue), "a.speed");
}

TEST_F(DiaAttributeParameterFileSet, ConstraintBreakingValueRejectedNamingBothKeys)
{
    // lo <= hi; hi is 8, so lo=9 breaks it.
    ASSERT_FALSE(pf.Set(StringCRC("lo"), Json::Value(9.0), err));
    EXPECT_STREQ(err.key.AsCStr(), "lo");
    EXPECT_STREQ(err.otherKey.AsCStr(), "hi");
    EXPECT_FLOAT_EQ(Base(pf, "lo"), 2.0f);
    EXPECT_TRUE(obs.names.empty());
    ASSERT_FALSE(pf.Set(StringCRC("hi"), Json::Value(1.0), err));
    EXPECT_STREQ(err.key.AsCStr(), "lo");
    EXPECT_STREQ(err.otherKey.AsCStr(), "hi");
    EXPECT_FLOAT_EQ(Base(pf, "hi"), 8.0f);
    EXPECT_TRUE(obs.names.empty());
}

TEST(DiaAttributeParameterFileSetNotLoaded, FailsCleanly)
{
    ParameterFile pf;
    ParamError err;
    EXPECT_FALSE(pf.Set(StringCRC("x"), Json::Value(1), err));
    EXPECT_FALSE(pf.IsLoaded());
}

// ---------------------------------------------------------------------------
// Reload (AC 11)
// ---------------------------------------------------------------------------
class DiaAttributeParameterFileReload : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(LoadFile(pf, FixtureText()));
        pf.Values().GetObserverSubject().Subscribe(&obs);
    }
    void TearDown() override { pf.Values().GetObserverSubject().Unsubscribe(&obs); }

    bool DoReload(const std::string& text, std::initializer_list<const Json::Value*> packs)
    {
        std::vector<const Json::Value*> v(packs);
        return pf.Reload(text.c_str(), v.data(), static_cast<uint32_t>(v.size()), warn, err);
    }

    ParameterFile pf;
    CountingObserver obs;
    ParamWarnings warn;
    ParamError err;
};

TEST_F(DiaAttributeParameterFileReload, ZeroPacksGivesDefaults)
{
    ASSERT_TRUE(pf.Set(StringCRC("a.speed"), Json::Value(9.0), err));
    obs.names.clear();
    ASSERT_TRUE(pf.Reload(FixtureText().c_str(), nullptr, 0, warn, err));
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 5.0f);   // Set value discarded
    ASSERT_EQ(obs.names.size(), 1u);              // only the changed one fires
    EXPECT_TRUE(obs.Saw("a.speed"));
    EXPECT_EQ(warn.items.Size(), 0u);
}

TEST_F(DiaAttributeParameterFileReload, TwoPacksEqualFreshLoadPlusApply)
{
    const Json::Value p1 = MakePack(R"("a.speed":7,"a.count":4,"m.temp":{"Desert":0.3})", MatchingBasedOn(pf));
    const Json::Value p2 = MakePack(R"("a.speed":8,"a.mode":"Fast")", MatchingBasedOn(pf));
    ASSERT_TRUE(pf.Set(StringCRC("lo"), Json::Value(1.0), err));   // discarded by the reload

    ASSERT_TRUE(DoReload(FixtureText(), { &p1, &p2 }));

    ParameterFile fresh;
    ASSERT_TRUE(LoadFile(fresh, FixtureText()));
    ParamWarnings w2; ParamError e2;
    ASSERT_TRUE(fresh.ApplyPack(p1, w2, e2));
    ASSERT_TRUE(fresh.ApplyPack(p2, w2, e2));
    EXPECT_TRUE(BitEqual(BaseValues(pf), BaseValues(fresh)));
    EXPECT_EQ(pf.SchemaHash(), fresh.SchemaHash());
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 8.0f);
    EXPECT_FLOAT_EQ(Base(pf, "lo"), 2.0f);
}

TEST_F(DiaAttributeParameterFileReload, WarningsFromPacksAreReported)
{
    const Json::Value p1 = MakePack(R"("nope":1)", MatchingBasedOn(pf));
    ASSERT_TRUE(DoReload(FixtureText(), { &p1 }));
    EXPECT_TRUE(HasWarningKey(warn, "nope"));
}

TEST_F(DiaAttributeParameterFileReload, FailingSecondPackLeavesStateIntact)
{
    ASSERT_TRUE(pf.Set(StringCRC("a.speed"), Json::Value(9.0), err));
    obs.names.clear();
    const std::vector<float> before = BaseValues(pf);
    const uint64_t hashBefore = pf.SchemaHash();
    const unsigned int countBefore = pf.Schema().GetAttributeCount();

    const std::string newText = Replace(FixtureText(), "\"a.count\"", "\"a.other\"");
    const Json::Value ok = MakePack(R"("a.speed":7)");
    const Json::Value bad = MakePack(R"("lo":9,"hi":1)");   // breaks lo <= hi
    EXPECT_FALSE(DoReload(newText, { &ok, &bad }));

    EXPECT_TRUE(BitEqual(before, BaseValues(pf)));
    EXPECT_EQ(hashBefore, pf.SchemaHash());
    EXPECT_EQ(countBefore, pf.Schema().GetAttributeCount());
    EXPECT_NE(pf.Schema().FindAttribute(StringCRC("a.count")), nullptr);
    EXPECT_EQ(pf.Schema().FindAttribute(StringCRC("a.other")), nullptr);
    EXPECT_TRUE(obs.names.empty());
    EXPECT_EQ(warn.items.Size(), 0u);
    EXPECT_NE(std::string(err.key.AsCStr()), "");
    ASSERT_TRUE(pf.Set(StringCRC("a.count"), Json::Value(6), err));   // still usable
}

TEST_F(DiaAttributeParameterFileReload, MalformedSidecarLeavesStateIntact)
{
    ASSERT_TRUE(pf.Set(StringCRC("a.speed"), Json::Value(9.0), err));
    obs.names.clear();
    const std::vector<float> before = BaseValues(pf);
    const uint64_t hashBefore = pf.SchemaHash();
    EXPECT_FALSE(pf.Reload("{ not json", nullptr, 0, warn, err));
    EXPECT_TRUE(BitEqual(before, BaseValues(pf)));
    EXPECT_EQ(hashBefore, pf.SchemaHash());
    EXPECT_TRUE(obs.names.empty());
    EXPECT_TRUE(pf.IsLoaded());
}

TEST_F(DiaAttributeParameterFileReload, ConstraintViolatingDefaultsLeaveStateIntact)
{
    const std::vector<float> before = BaseValues(pf);
    const std::string text = MakeSidecarText({ SidecarFloat("lo", 0, 10, 9), SidecarFloat("hi", 0, 10, 1) },
                                             R"([{"a":"lo","op":"<=","b":"hi"}])", "test.params");
    EXPECT_FALSE(pf.Reload(text.c_str(), nullptr, 0, warn, err));
    EXPECT_TRUE(BitEqual(before, BaseValues(pf)));
}

TEST_F(DiaAttributeParameterFileReload, SchemaHashChangesWhenSidecarChanges)
{
    const uint64_t before = pf.SchemaHash();
    const std::string text = Replace(FixtureText(), "\"a.count\"", "\"a.other\"");
    ASSERT_TRUE(DoReload(text, {}));
    EXPECT_NE(before, pf.SchemaHash());
    ParameterFile fresh;
    ASSERT_TRUE(LoadFile(fresh, text));
    EXPECT_EQ(fresh.SchemaHash(), pf.SchemaHash());
    EXPECT_NE(pf.Schema().FindAttribute(StringCRC("a.other")), nullptr);
    EXPECT_EQ(pf.Schema().FindAttribute(StringCRC("a.count")), nullptr);
    EXPECT_EQ(pf.Values().GetAttributeCount(), pf.Schema().GetAttributeCount());
    ASSERT_TRUE(pf.ApplyPack(MakePack(R"("a.other":4)", MatchingBasedOn(pf)), warn, err));   // new schema is live
    EXPECT_FLOAT_EQ(Base(pf, "a.other"), 4.0f);
}

namespace
{
    AttributeModifier AddMod(const char* name, const char* attr, float v)
    {
        AttributeModifier m{};
        m.modifier_name = StringCRC(name);
        m.attribute_name = StringCRC(attr);
        m.operation = ModifierOperation::Add;
        m.value = v;
        return m;
    }
}

TEST_F(DiaAttributeParameterFileReload, ModifierKeptAndHandleStaysValid)
{
    const ModifierHandle h = pf.Values().AddModifier(AddMod("boost", "a.speed", 2.0f));
    const Json::Value p = MakePack(R"("a.speed":6)", MatchingBasedOn(pf));
    obs.names.clear();
    ASSERT_TRUE(DoReload(FixtureText(), { &p }));
    EXPECT_FLOAT_EQ(pf.Values().GetValue(StringCRC("a.speed")), 8.0f);   // 6 + 2
    EXPECT_FLOAT_EQ(Base(pf, "a.speed"), 6.0f);
    EXPECT_EQ(pf.Values().GetModifierCountForAttribute(StringCRC("a.speed")), 1u);
    EXPECT_EQ(warn.items.Size(), 0u);
    ASSERT_EQ(obs.names.size(), 1u);   // resolved 7 -> 8

    pf.Values().RemoveModifier(h);
    EXPECT_EQ(pf.Values().GetModifierCountForAttribute(StringCRC("a.speed")), 0u);
    EXPECT_FLOAT_EQ(pf.Values().GetValue(StringCRC("a.speed")), 6.0f);
}

TEST_F(DiaAttributeParameterFileReload, ModifierKeptWhenAttributeIndexMoves)
{
    const ModifierHandle h = pf.Values().AddModifier(AddMod("boost", "lo", 1.0f));
    const std::string text = MakeSidecarText({   // a.speed gone, so every later attribute shifts index
        SidecarInt("a.count", 0, 10, 3),
        SidecarFloat("lo", 0, 10, 2),
        SidecarFloat("hi", 0, 10, 8)
    }, R"([{"a":"lo","op":"<=","b":"hi"}])", "test.params");
    ASSERT_TRUE(DoReload(text, {}));
    EXPECT_FLOAT_EQ(pf.Values().GetValue(StringCRC("lo")), 3.0f);
    pf.Values().RemoveModifier(h);
    EXPECT_FLOAT_EQ(pf.Values().GetValue(StringCRC("lo")), 2.0f);
}

TEST_F(DiaAttributeParameterFileReload, SchemaGrowsAndShrinks)
{
    const ModifierHandle h = pf.Values().AddModifier(AddMod("boost", "hi", 1.0f));
    std::vector<std::string> entries;
    for (int i = 0; i < 20; ++i) entries.push_back(SidecarFloat(("extra" + std::to_string(i)).c_str(), 0, 10, 1));
    entries.push_back(SidecarFloat("lo", 0, 10, 2));
    entries.push_back(SidecarFloat("hi", 0, 10, 8));
    std::string body;
    for (const std::string& e : entries) { if (!body.empty()) body += ","; body += e; }
    const std::string big = std::string("{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"test.params\",\"entries\":[") + body + "]}";
    ASSERT_TRUE(DoReload(big, {}));
    EXPECT_EQ(pf.Values().GetAttributeCount(), 22u);
    EXPECT_FLOAT_EQ(pf.Values().GetValue(StringCRC("hi")), 9.0f);
    EXPECT_FLOAT_EQ(Base(pf, "extra19"), 1.0f);
    ASSERT_TRUE(pf.Set(StringCRC("extra19"), Json::Value(4), err));
    EXPECT_FLOAT_EQ(Base(pf, "extra19"), 4.0f);

    ASSERT_TRUE(DoReload(FixtureText(), {}));   // shrink back
    EXPECT_EQ(pf.Values().GetAttributeCount(), pf.Schema().GetAttributeCount());
    EXPECT_FLOAT_EQ(pf.Values().GetValue(StringCRC("hi")), 9.0f);
    pf.Values().RemoveModifier(h);
    EXPECT_FLOAT_EQ(pf.Values().GetValue(StringCRC("hi")), 8.0f);
}

TEST_F(DiaAttributeParameterFileReload, ModifierOnRemovedAttributeDroppedWithWarning)
{
    const ModifierHandle keep = pf.Values().AddModifier(AddMod("keep", "a.speed", 1.0f));
    pf.Values().AddModifier(AddMod("gone", "a.count", 1.0f));
    const std::string text = Replace(FixtureText(), "\"a.count\"", "\"a.other\"");
    ASSERT_TRUE(DoReload(text, {}));
    EXPECT_TRUE(HasWarningKey(warn, "a.count"));
    EXPECT_EQ(pf.Values().GetModifierCountForAttribute(StringCRC("a.speed")), 1u);
    EXPECT_EQ(pf.Values().GetModifierCountForAttribute(StringCRC("a.other")), 0u);
    pf.Values().RemoveModifier(keep);
    EXPECT_FLOAT_EQ(pf.Values().GetValue(StringCRC("a.speed")), 5.0f);
}

TEST(DiaAttributeParameterFileReloadNotLoaded, BehavesLikeLoadPlusPacks)
{
    ParameterFile pf;
    const std::string text = MakeSidecarText({ SidecarFloat("x", 0, 10, 3) }, nullptr, "test.params");
    const Json::Value p = MakePack(R"("x":7)");
    const Json::Value* packs[] = { &p };
    ParamWarnings w; ParamError e;
    ASSERT_TRUE(pf.Reload(text.c_str(), packs, 1, w, e));
    EXPECT_TRUE(pf.IsLoaded());
    EXPECT_FLOAT_EQ(Base(pf, "x"), 7.0f);
    ParameterFile bad;
    EXPECT_FALSE(bad.Reload("{ nope", nullptr, 0, w, e));
    EXPECT_FALSE(bad.IsLoaded());
}


// ===========================================================================
// ToJson (AC 12)
// ===========================================================================
namespace
{
    namespace fs = std::filesystem;

    std::string ToText(const Json::Value& v) { Json::FastWriter w; return w.write(v); }

    bool HasExactly(const Json::Value& v, std::vector<std::string> names)
    {
        std::vector<std::string> got = v.getMemberNames();
        std::sort(got.begin(), got.end());
        std::sort(names.begin(), names.end());
        return got == names;
    }

    bool SetOk(ParameterFile& pf, const char* key, const Json::Value& v)
    {
        ParamError e;
        return pf.Set(StringCRC(key), v, e);
    }

    // Loads `text` into `fresh`, applies `pack`; returns whether the pack applied.
    bool ApplyToFresh(ParameterFile& fresh, const std::string& text, const Json::Value& pack, ParamWarnings* warnOut = nullptr)
    {
        if (!LoadFile(fresh, text)) return false;
        ParamWarnings w; ParamError e;
        const bool ok = fresh.ApplyPack(pack, w, e);
        if (warnOut != nullptr) *warnOut = w;
        return ok;
    }

    fs::path FindConformanceDataDir()
    {
        std::error_code ec;
        fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const fs::path candidate = dir / "Dia" / "DiaValueSchema" / "Testing" / "Conformance" / "data";
            if (fs::exists(candidate, ec)) return candidate;
            const fs::path parent = dir.parent_path();
            if (parent == dir) break;
            dir = parent;
        }
        return fs::path();
    }

    std::string ZeroText()
    {
        return MakeSidecarText({ SidecarFloat("z", -1, 1, 0.5), SidecarFloat("y", -1, 1, 0) }, nullptr, "test.params");
    }

    std::string SmallText()
    {
        return MakeSidecarText({ SidecarFloat("x", 0, 10, 5), SidecarInt("y", 0, 10, 3) }, nullptr, "test.params");
    }
}

TEST(DiaAttributeParameterFileToJson, FullListsEveryParameterWithKinds)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    const Json::Value j = pf.ToJson(JsonShape::Full);
    ASSERT_TRUE(j.isObject());
    EXPECT_TRUE(HasExactly(j, { "format", "version", "basedOn", "a.speed", "a.count", "a.flag", "a.mode", "lo", "hi", "m.temp" }));
    EXPECT_EQ(j["format"].asString(), "test.params");
    EXPECT_EQ(j["version"].asInt(), 1);
    EXPECT_EQ(j["basedOn"].asString(), HexOf(pf.SchemaHash()));
    EXPECT_TRUE(j["a.speed"].isDouble());
    EXPECT_DOUBLE_EQ(j["a.speed"].asDouble(), 5.0);
    EXPECT_TRUE(j["a.count"].isIntegral());
    EXPECT_EQ(j["a.count"].asInt(), 3);
    ASSERT_TRUE(j["a.flag"].isBool());
    EXPECT_FALSE(j["a.flag"].asBool());
    ASSERT_TRUE(j["a.mode"].isString());
    EXPECT_EQ(j["a.mode"].asString(), "Slow");
    ASSERT_TRUE(j["m.temp"].isObject());
    EXPECT_TRUE(HasExactly(j["m.temp"], { "TemperateForest", "Desert", "Tundra" }));
    EXPECT_NEAR(j["m.temp"]["Desert"].asDouble(), 0.8, 1e-6);
}

TEST(DiaAttributeParameterFileToJson, FullReflectsCurrentValuesAsEnumIdsAndBools)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    ASSERT_TRUE(SetOk(pf, "a.mode", Json::Value("Turbo")));
    ASSERT_TRUE(SetOk(pf, "a.flag", Json::Value(true)));
    ASSERT_TRUE(SetOk(pf, "m.temp.Tundra", Json::Value(0.25)));
    const Json::Value j = pf.ToJson(JsonShape::Full);
    EXPECT_EQ(j["a.mode"].asString(), "Turbo");
    EXPECT_TRUE(j["a.flag"].isBool());
    EXPECT_TRUE(j["a.flag"].asBool());
    EXPECT_DOUBLE_EQ(j["m.temp"]["Tundra"].asDouble(), 0.25);
}

TEST(DiaAttributeParameterFileToJson, FullThenApplyPackOnFreshLoadIsIdentity)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(7.3)));
    ASSERT_TRUE(SetOk(pf, "a.count", Json::Value(9)));
    ASSERT_TRUE(SetOk(pf, "a.mode", Json::Value("Fast")));
    ASSERT_TRUE(SetOk(pf, "m.temp.Desert", Json::Value(0.123456789)));
    ParameterFile fresh; ParamWarnings w;
    ASSERT_TRUE(ApplyToFresh(fresh, FixtureText(), pf.ToJson(JsonShape::Full), &w));
    EXPECT_EQ(w.items.Size(), 0u);
    EXPECT_TRUE(BitEqual(BaseValues(pf), BaseValues(fresh)));
}

TEST(DiaAttributeParameterFileToJson, DiffOfDefaultsIsEnvelopeOnlyAndValid)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    const Json::Value j = pf.ToJson(JsonShape::Diff);
    EXPECT_TRUE(HasExactly(j, { "format", "version", "basedOn" }));
    EXPECT_EQ(j["basedOn"].asString(), HexOf(pf.SchemaHash()));
    ParameterFile fresh; ParamWarnings w;
    ASSERT_TRUE(ApplyToFresh(fresh, FixtureText(), j, &w));
    EXPECT_EQ(w.items.Size(), 0u);   // basedOn matches: same hex format ApplyPack parses
    EXPECT_TRUE(BitEqual(BaseValues(pf), BaseValues(fresh)));
}

TEST(DiaAttributeParameterFileToJson, DiffAfterTwoSetsHoldsExactlyThoseTwoPlusEnvelope)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(7.0)));
    ASSERT_TRUE(SetOk(pf, "a.count", Json::Value(4)));
    const Json::Value j = pf.ToJson(JsonShape::Diff);
    EXPECT_TRUE(HasExactly(j, { "format", "version", "basedOn", "a.speed", "a.count" }));
    EXPECT_DOUBLE_EQ(j["a.speed"].asDouble(), 7.0);
    EXPECT_EQ(j["a.count"].asInt(), 4);
}

TEST(DiaAttributeParameterFileToJson, SetBackToDefaultLeavesDiff)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(7.0)));
    ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(5.0)));
    EXPECT_TRUE(HasExactly(pf.ToJson(JsonShape::Diff), { "format", "version", "basedOn" }));
}

TEST(DiaAttributeParameterFileToJson, DiffNestsOnlyChangedMapColumns)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    ASSERT_TRUE(SetOk(pf, "m.temp.Desert", Json::Value(0.3)));
    const Json::Value j = pf.ToJson(JsonShape::Diff);
    EXPECT_TRUE(HasExactly(j, { "format", "version", "basedOn", "m.temp" }));
    ASSERT_TRUE(j["m.temp"].isObject());
    EXPECT_TRUE(HasExactly(j["m.temp"], { "Desert" }));
    EXPECT_NEAR(j["m.temp"]["Desert"].asDouble(), 0.3, 1e-7);
    ParameterFile fresh;
    ASSERT_TRUE(ApplyToFresh(fresh, FixtureText(), j));
    EXPECT_TRUE(BitEqual(BaseValues(pf), BaseValues(fresh)));
}

TEST(DiaAttributeParameterFileToJson, DiffWritesEnumIdAndBool)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    ASSERT_TRUE(SetOk(pf, "a.mode", Json::Value("Turbo")));
    ASSERT_TRUE(SetOk(pf, "a.flag", Json::Value(true)));
    const Json::Value j = pf.ToJson(JsonShape::Diff);
    EXPECT_TRUE(HasExactly(j, { "format", "version", "basedOn", "a.mode", "a.flag" }));
    ASSERT_TRUE(j["a.mode"].isString());
    EXPECT_EQ(j["a.mode"].asString(), "Turbo");
    ASSERT_TRUE(j["a.flag"].isBool());
    EXPECT_TRUE(j["a.flag"].asBool());
}

TEST(DiaAttributeParameterFileToJson, DiffRoundTripsFloatsBitForBitThroughText)
{
    const float values[] = { 0.1f, 1.00000012f, 3.14159274f, 1e-7f, 9.99999905f, 0.333333343f, 7.0f, 1.23456789f };
    for (const float v : values)
    {
        SCOPED_TRACE(v);
        ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
        ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(static_cast<double>(v))));
        ASSERT_EQ(Base(pf, "a.speed"), v);
        const std::string text = ToText(pf.ToJson(JsonShape::Diff));
        Json::Value reparsed; Json::Reader reader;
        ASSERT_TRUE(reader.parse(text, reparsed)) << text;
        ParameterFile fresh;
        ASSERT_TRUE(ApplyToFresh(fresh, FixtureText(), reparsed));
        EXPECT_TRUE(BitEqual(BaseValues(pf), BaseValues(fresh))) << text;
    }
}

TEST(DiaAttributeParameterFileToJson, DiffUsesShortestDecimalForFloats)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(0.1)));   // stored as float 0.1f
    // The double is the one nearest the shortest decimal "0.1", not the float widened (0.10000000149011612).
    EXPECT_EQ(pf.ToJson(JsonShape::Diff)["a.speed"].asDouble(), 0.1);
}

TEST(DiaAttributeParameterFileToJson, NegativeZeroSurvivesInMemoryRoundTrip)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, ZeroText()));
    const float vals[2] = { -0.0f, -0.0f };
    ASSERT_TRUE(pf.Values().ReplaceBaseValues(vals, 2));
    ASSERT_TRUE(std::signbit(pf.Values().GetBaseValueByIndex(1)));
    const Json::Value j = pf.ToJson(JsonShape::Diff);
    EXPECT_TRUE(HasExactly(j, { "format", "version", "basedOn", "z", "y" }));   // -0.0 differs bitwise from the +0.0 default
    ParameterFile fresh;
    ASSERT_TRUE(ApplyToFresh(fresh, ZeroText(), j));
    EXPECT_TRUE(BitEqual(BaseValues(pf), BaseValues(fresh)));
}

TEST(DiaAttributeParameterFileToJson, EmptySidecarGivesEnvelopeOnly)
{
    const std::string text = MakeSidecarText({}, nullptr, "test.params");
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, text));
    EXPECT_TRUE(HasExactly(pf.ToJson(JsonShape::Full), { "format", "version", "basedOn" }));
    EXPECT_TRUE(HasExactly(pf.ToJson(JsonShape::Diff), { "format", "version", "basedOn" }));
    ParameterFile fresh; ParamWarnings w;
    ASSERT_TRUE(ApplyToFresh(fresh, text, pf.ToJson(JsonShape::Diff), &w));
    EXPECT_EQ(w.items.Size(), 0u);
}

TEST(DiaAttributeParameterFileToJson, NotLoadedGivesEmptyObject)
{
    ParameterFile pf;
    EXPECT_TRUE(pf.ToJson(JsonShape::Full).isObject());
    EXPECT_EQ(pf.ToJson(JsonShape::Full).size(), 0u);
    EXPECT_EQ(pf.ToJson(JsonShape::Diff).size(), 0u);
}

TEST(DiaAttributeParameterFileToJson, DiffMatchesCommittedConformanceFixture)
{
    const fs::path dir = FindConformanceDataDir();
    ASSERT_FALSE(dir.empty());
    std::ifstream in(dir / "parameter-file-to-json-diff.case.json", std::ios::binary);
    ASSERT_TRUE(in.good());
    std::stringstream ss; ss << in.rdbuf();
    Json::Value c; Json::Reader reader;
    ASSERT_TRUE(reader.parse(ss.str(), c)) << reader.getFormattedErrorMessages();

    const std::string sidecar = ToText(c["schemas"][0u]);
    const Json::Value* pack = nullptr;
    for (Json::ArrayIndex i = 0; i < c["files"].size(); ++i)
        if (c["files"][i]["pack"].asBool()) pack = &c["files"][i]["content"];
    ASSERT_NE(pack, nullptr);

    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, sidecar));
    ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(7.25)));
    ASSERT_TRUE(SetOk(pf, "a.count", Json::Value(4)));
    ASSERT_TRUE(SetOk(pf, "a.flag", Json::Value(true)));
    ASSERT_TRUE(SetOk(pf, "a.mode", Json::Value("Fast")));
    ASSERT_TRUE(SetOk(pf, "m.temp.Desert", Json::Value(0.3)));
    const Json::Value diff = pf.ToJson(JsonShape::Diff);
    EXPECT_EQ(diff, *pack) << "actual: " << ToText(diff);

    ParameterFile fresh; ParamWarnings w;
    ASSERT_TRUE(ApplyToFresh(fresh, sidecar, *pack, &w));
    EXPECT_EQ(w.items.Size(), 0u);
    EXPECT_TRUE(BitEqual(BaseValues(pf), BaseValues(fresh)));
}

// ===========================================================================
// Hash (AC 13)
// ===========================================================================
TEST(DiaAttributeParameterFileHash, SameAcrossTwoLoads)
{
    ParameterFile a, b;
    ASSERT_TRUE(LoadFile(a, FixtureText())); ASSERT_TRUE(LoadFile(b, FixtureText()));
    EXPECT_EQ(a.Hash(), b.Hash());
    EXPECT_NE(a.Hash(), 0u);
}

TEST(DiaAttributeParameterFileHash, PinnedLiteralForSmallFixture)
{
    // FNV-1a 64 over the float32 LE bytes of [5.0, 3.0]; cross-checked with an independent Python computation.
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, SmallText()));
    EXPECT_EQ(pf.Hash(), 0x9e36cdbe1c6a8665ull);
}

TEST(DiaAttributeParameterFileHash, EmptySidecarIsTheFnvOffsetBasis)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, MakeSidecarText({}, nullptr, "test.params")));
    EXPECT_EQ(pf.Hash(), 0xcbf29ce484222325ull);
}

TEST(DiaAttributeParameterFileHash, ChangingAnySingleValueChangesIt)
{
    ParameterFile ref; ASSERT_TRUE(LoadFile(ref, FixtureText()));
    const uint64_t baseline = ref.Hash();
    const uint32_t n = ref.Values().GetAttributeCount();
    ASSERT_GE(n, 8u);
    for (uint32_t i = 0; i < n; ++i)
    {
        ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
        const AttributeDefinition& d = pf.Schema().GetAttributeByIndex(i);
        const float v = pf.Values().GetBaseValueByIndex(i);
        const float to = (v + 1.0f <= d.maximum_value) ? v + 1.0f : v - 1.0f;
        std::vector<float> vals = BaseValues(pf);
        vals[i] = to;
        ASSERT_TRUE(pf.Values().ReplaceBaseValues(vals.data(), n));
        EXPECT_NE(pf.Hash(), baseline) << "index " << i << " (" << d.fullKey.AsCStr() << ")";
    }
}

TEST(DiaAttributeParameterFileHash, NegativeZeroEqualsPositiveZero)
{
    ParameterFile a, b;
    ASSERT_TRUE(LoadFile(a, ZeroText())); ASSERT_TRUE(LoadFile(b, ZeroText()));
    const float pos[2] = { 0.0f, 0.0f };
    const float neg[2] = { -0.0f, -0.0f };
    ASSERT_TRUE(a.Values().ReplaceBaseValues(pos, 2));
    ASSERT_TRUE(b.Values().ReplaceBaseValues(neg, 2));
    ASSERT_TRUE(std::signbit(b.Values().GetBaseValueByIndex(0)));
    EXPECT_EQ(a.Hash(), b.Hash());
}

TEST(DiaAttributeParameterFileHash, ModifiersDoNotChangeIt)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    const uint64_t before = pf.Hash();
    AttributeModifier m{};
    m.modifier_name = StringCRC("buff"); m.attribute_name = StringCRC("a.speed");
    m.operation = ModifierOperation::Add; m.value = 2.0f; m.when_condition[0] = '\0';
    const ModifierHandle h = pf.Values().AddModifier(m);
    EXPECT_NE(pf.Values().GetValue(StringCRC("a.speed")), 5.0f);
    EXPECT_EQ(pf.Hash(), before);
    pf.Values().RemoveModifier(h);
    EXPECT_EQ(pf.Hash(), before);
}

TEST(DiaAttributeParameterFileHash, SetThenSetBack)
{
    ParameterFile pf; ASSERT_TRUE(LoadFile(pf, FixtureText()));
    const uint64_t before = pf.Hash();
    ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(6.0)));
    EXPECT_NE(pf.Hash(), before);
    ASSERT_TRUE(SetOk(pf, "a.speed", Json::Value(5.0)));
    EXPECT_EQ(pf.Hash(), before);
}

TEST(DiaAttributeParameterFileHash, NotLoadedIsTheOffsetBasis)
{
    ParameterFile pf;
    EXPECT_EQ(pf.Hash(), 0xcbf29ce484222325ull);
}

// ===========================================================================
// Save round trip (AC 17): the existing AttributeSet ISaveable is enough
// ===========================================================================
TEST(DiaAttributeParameterFileSave, RoundTripRestoresBaseValuesModifiersAndHash)
{
    ParameterFile src; ASSERT_TRUE(LoadFile(src, FixtureText()));
    ASSERT_TRUE(SetOk(src, "a.speed", Json::Value(7.3)));
    ASSERT_TRUE(SetOk(src, "a.mode", Json::Value("Turbo")));
    ASSERT_TRUE(SetOk(src, "a.flag", Json::Value(true)));
    ASSERT_TRUE(SetOk(src, "m.temp.Desert", Json::Value(0.123456789)));
    AttributeModifier add{};
    add.modifier_name = StringCRC("buff"); add.attribute_name = StringCRC("a.speed");
    add.operation = ModifierOperation::Add; add.value = 1.5f; add.when_condition[0] = '\0';
    AttributeModifier mul = add;
    mul.modifier_name = StringCRC("slow"); mul.attribute_name = StringCRC("m.temp.Desert");
    mul.operation = ModifierOperation::Multiply; mul.value = 0.5f;
    (void)src.Values().AddModifier(add);
    (void)src.Values().AddModifier(mul);

    Dia::SaveGame::SaveContext save;
    src.Values().Serialize(save);
    static char buf[Dia::SaveGame::SaveContext::kBufferSize];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));
    Json::Value root; Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    ParameterFile dst; ASSERT_TRUE(LoadFile(dst, FixtureText()));
    ASSERT_NE(dst.Hash(), src.Hash());
    Dia::SaveGame::LoadContext load(root);
    dst.Values().Deserialize(load);

    EXPECT_TRUE(BitEqual(BaseValues(src), BaseValues(dst)));
    EXPECT_EQ(dst.Hash(), src.Hash());
    EXPECT_EQ(dst.Values().GetModifierCountForAttribute(StringCRC("a.speed")), 1u);
    EXPECT_EQ(dst.Values().GetModifierCountForAttribute(StringCRC("m.temp.Desert")), 1u);
    EXPECT_EQ(dst.Values().GetModifierCountForAttribute(StringCRC("a.count")), 0u);
    EXPECT_EQ(dst.Values().GetValue(StringCRC("a.speed")), src.Values().GetValue(StringCRC("a.speed")));
    EXPECT_EQ(dst.Values().GetValue(StringCRC("m.temp.Desert")), src.Values().GetValue(StringCRC("m.temp.Desert")));
}
