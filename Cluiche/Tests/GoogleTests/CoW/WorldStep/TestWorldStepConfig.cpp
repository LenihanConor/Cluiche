// World Step Feature 1: config loader (AC 1-4).
#include <gtest/gtest.h>
#include <Shared/WorldStep/WorldStepConfig.h>

#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <memory>
#include <string>

using namespace CoW::WorldStep;
using Dia::Core::StringCRC;

namespace
{
    Json::Value Parse(const char* text)
    {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errors;
        const char* end = text + std::strlen(text);
        EXPECT_TRUE(reader->parse(text, end, &root, &errors)) << text << " : " << errors;
        return root;
    }

    // out must equal a default-constructed config.
    void ExpectDefault(const WorldStepConfig& c)
    {
        EXPECT_EQ(c.order.Size(), 0u);
        EXPECT_EQ(c.applyAfterDays, 7u);
        EXPECT_TRUE(c.deferred);
    }

    struct FaultRow
    {
        const char* name;
        const char* json;
        const char* expectedKey;
    };

    // One row per fault listed in AC 2.
    const FaultRow kFaults[] = {
        {"OrderMissing",        R"({"apply_after_days":7})",                                   "order"},
        {"OrderEmpty",          R"({"order":[]})",                                             "order"},
        {"OrderNotArray",       R"({"order":"weather"})",                                      "order"},
        {"OrderNine",           R"({"order":["a","b","c","d","e","f","g","h","i"]})",          "order"},
        {"OrderNonString",      R"({"order":["weather",3]})",                                  "order"},
        {"OrderEmptyString",    R"({"order":["weather",""]})",                                 "order"},
        {"OrderDuplicate",      R"({"order":["weather","ecology","weather"]})",                "order"},
        {"DaysZero",            R"({"order":["weather"],"apply_after_days":0})",               "apply_after_days"},
        {"Days366",             R"({"order":["weather"],"apply_after_days":366})",             "apply_after_days"},
        {"DaysFraction",        R"({"order":["weather"],"apply_after_days":7.5})",             "apply_after_days"},
        {"DaysString",          R"({"order":["weather"],"apply_after_days":"7"})",             "apply_after_days"},
        {"DeferredString",      R"({"order":["weather"],"deferred":"true"})",                  "deferred"},
        {"UnknownTopLevelKey",  R"({"order":["weather"],"aply_after_days":7})",                "aply_after_days"},
    };

    class WorldStepConfigFault : public ::testing::TestWithParam<FaultRow> {};

    StringCRC ConstantHash(const char*) { return StringCRC("collide"); }
}

TEST(WorldStepConfig, ValidConfigLoads)   // AC 1
{
    WorldStepConfig out;
    ConfigError err;
    ASSERT_TRUE(LoadWorldStepConfig(
        Parse(R"({"order":["weather","ecology"],"apply_after_days":7,"deferred":true})"), out, err));
    ASSERT_EQ(out.order.Size(), 2u);
    EXPECT_TRUE(out.order[0].crc == StringCRC("weather"));
    EXPECT_TRUE(out.order[0].name == "weather");
    EXPECT_TRUE(out.order[1].crc == StringCRC("ecology"));
    EXPECT_TRUE(out.order[1].name == "ecology");
    EXPECT_EQ(out.applyAfterDays, 7u);
    EXPECT_TRUE(out.deferred);
}

TEST(WorldStepConfig, MissingOptionalKeysTakeDefaults)   // AC 1
{
    WorldStepConfig out;
    out.applyAfterDays = 99;
    out.deferred = false;
    ConfigError err;
    ASSERT_TRUE(LoadWorldStepConfig(Parse(R"({"order":["weather"]})"), out, err));
    EXPECT_EQ(out.order.Size(), 1u);
    EXPECT_EQ(out.applyAfterDays, 7u);
    EXPECT_TRUE(out.deferred);
}

TEST(WorldStepConfig, DeferredFalseIsAccepted)
{
    WorldStepConfig out;
    ConfigError err;
    ASSERT_TRUE(LoadWorldStepConfig(Parse(R"({"order":["weather"],"deferred":false})"), out, err));
    EXPECT_FALSE(out.deferred);
}

TEST(WorldStepConfig, ApplyAfterDaysBoundsAreValid)   // AC 4
{
    WorldStepConfig out;
    ConfigError err;
    ASSERT_TRUE(LoadWorldStepConfig(Parse(R"({"order":["weather"],"apply_after_days":1})"), out, err));
    EXPECT_EQ(out.applyAfterDays, 1u);
    ASSERT_TRUE(LoadWorldStepConfig(Parse(R"({"order":["weather"],"apply_after_days":365})"), out, err));
    EXPECT_EQ(out.applyAfterDays, 365u);
}

TEST(WorldStepConfig, EightStagesIsValid)
{
    WorldStepConfig out;
    ConfigError err;
    ASSERT_TRUE(LoadWorldStepConfig(Parse(R"({"order":["a","b","c","d","e","f","g","h"]})"), out, err));
    EXPECT_EQ(out.order.Size(), 8u);
}

TEST_P(WorldStepConfigFault, RejectsAllOrNothingAndNamesKey)   // AC 2
{
    const FaultRow& row = GetParam();
    WorldStepConfig out;
    // Pre-dirty so we can prove out is reset, not merely untouched.
    out.applyAfterDays = 99;
    out.deferred = false;
    StageId stale;
    stale.crc = StringCRC("stale");
    out.order.Add(stale);
    ConfigError err;

    EXPECT_FALSE(LoadWorldStepConfig(Parse(row.json), out, err)) << row.name;
    ExpectDefault(out);
    EXPECT_TRUE(err.key == row.expectedKey) << row.name << " key was '" << err.key.AsCStr() << "'";
    EXPECT_FALSE(err.reason.IsEmpty()) << row.name;
}

INSTANTIATE_TEST_SUITE_P(WorldStepConfig, WorldStepConfigFault, ::testing::ValuesIn(kFaults),
    [](const ::testing::TestParamInfo<FaultRow>& info) { return std::string(info.param.name); });

TEST(WorldStepConfig, NonObjectRootIsRejected)
{
    WorldStepConfig out;
    ConfigError err;
    EXPECT_FALSE(LoadWorldStepConfig(Parse(R"([1,2])"), out, err));
    ExpectDefault(out);
    EXPECT_FALSE(err.key.IsEmpty());
}

TEST(WorldStepConfig, LaterFaultStillLeavesOrderDefault)   // all-or-nothing: valid order, bad days
{
    WorldStepConfig out;
    ConfigError err;
    EXPECT_FALSE(LoadWorldStepConfig(Parse(R"({"order":["weather","ecology"],"apply_after_days":0})"), out, err));
    ExpectDefault(out);
}

TEST(WorldStepConfig, HashCollisionIsRejected)   // AC 3
{
    WorldStepConfig out;
    ConfigError err;
    EXPECT_FALSE(LoadWorldStepConfig(Parse(R"({"order":["weather","ecology"]})"), out, err, &ConstantHash));
    ExpectDefault(out);
    EXPECT_TRUE(err.key == "order");
}

TEST(WorldStepConfig, DefaultHashDoesNotCollideOnDistinctIds)
{
    WorldStepConfig out;
    ConfigError err;
    EXPECT_TRUE(LoadWorldStepConfig(Parse(R"({"order":["weather","hydro","ecology"]})"), out, err));
}
