// config loader (AC 1-4).
#include <gtest/gtest.h>
#include <DiaDeferredStep/StepChainConfig.h>
#include <DiaDeferredStep/StepChainConfigLoader.h>

#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <memory>
#include <string>

using namespace Dia::DeferredStep;
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
    void ExpectDefault(const StepChainConfig<>& c)
    {
        EXPECT_EQ(c.order.Size(), 0u);
        EXPECT_EQ(c.applyAfterSteps, 7u);
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
        {"OrderMissing",        R"({"apply_after_steps":7})",                                   "order"},
        {"OrderEmpty",          R"({"order":[]})",                                             "order"},
        {"OrderNotArray",       R"({"order":"weather"})",                                      "order"},
        {"OrderNine",           R"({"order":["a","b","c","d","e","f","g","h","i"]})",          "order"},
        {"OrderNonString",      R"({"order":["weather",3]})",                                  "order"},
        {"OrderEmptyString",    R"({"order":["weather",""]})",                                 "order"},
        {"OrderDuplicate",      R"({"order":["weather","ecology","weather"]})",                "order"},
        {"DaysZero",            R"({"order":["weather"],"apply_after_steps":0})",               "apply_after_steps"},
        {"Days366",             R"({"order":["weather"],"apply_after_steps":366})",             "apply_after_steps"},
        {"DaysFraction",        R"({"order":["weather"],"apply_after_steps":7.5})",             "apply_after_steps"},
        {"DaysString",          R"({"order":["weather"],"apply_after_steps":"7"})",             "apply_after_steps"},
        {"DeferredString",      R"({"order":["weather"],"deferred":"true"})",                  "deferred"},
        {"UnknownTopLevelKey",  R"({"order":["weather"],"aply_after_days":7})",                "aply_after_days"},
    };

    class StepChainConfigFault : public ::testing::TestWithParam<FaultRow> {};

    StringCRC ConstantHash(const char*) { return StringCRC("collide"); }
}

TEST(StepChainConfig, ValidConfigLoads)   // AC 1
{
    StepChainConfig<> out;
    ConfigError err;
    ASSERT_TRUE(LoadStepChainConfig(
        Parse(R"({"order":["weather","ecology"],"apply_after_steps":7,"deferred":true})"), out, err));
    ASSERT_EQ(out.order.Size(), 2u);
    EXPECT_TRUE(out.order[0].crc == StringCRC("weather"));
    EXPECT_TRUE(out.order[0].name == "weather");
    EXPECT_TRUE(out.order[1].crc == StringCRC("ecology"));
    EXPECT_TRUE(out.order[1].name == "ecology");
    EXPECT_EQ(out.applyAfterSteps, 7u);
    EXPECT_TRUE(out.deferred);
}

TEST(StepChainConfig, MissingOptionalKeysTakeDefaults)   // AC 1
{
    StepChainConfig<> out;
    out.applyAfterSteps = 99;
    out.deferred = false;
    ConfigError err;
    ASSERT_TRUE(LoadStepChainConfig(Parse(R"({"order":["weather"]})"), out, err));
    EXPECT_EQ(out.order.Size(), 1u);
    EXPECT_EQ(out.applyAfterSteps, 7u);
    EXPECT_TRUE(out.deferred);
}

TEST(StepChainConfig, DeferredFalseIsAccepted)
{
    StepChainConfig<> out;
    ConfigError err;
    ASSERT_TRUE(LoadStepChainConfig(Parse(R"({"order":["weather"],"deferred":false})"), out, err));
    EXPECT_FALSE(out.deferred);
}

TEST(StepChainConfig, ApplyAfterDaysBoundsAreValid)   // AC 4
{
    StepChainConfig<> out;
    ConfigError err;
    ASSERT_TRUE(LoadStepChainConfig(Parse(R"({"order":["weather"],"apply_after_steps":1})"), out, err));
    EXPECT_EQ(out.applyAfterSteps, 1u);
    ASSERT_TRUE(LoadStepChainConfig(Parse(R"({"order":["weather"],"apply_after_steps":365})"), out, err));
    EXPECT_EQ(out.applyAfterSteps, 365u);
}

TEST(StepChainConfig, EightStagesIsValid)
{
    StepChainConfig<> out;
    ConfigError err;
    ASSERT_TRUE(LoadStepChainConfig(Parse(R"({"order":["a","b","c","d","e","f","g","h"]})"), out, err));
    EXPECT_EQ(out.order.Size(), 8u);
}

TEST_P(StepChainConfigFault, RejectsAllOrNothingAndNamesKey)   // AC 2
{
    const FaultRow& row = GetParam();
    StepChainConfig<> out;
    // Pre-dirty so we can prove out is reset, not merely untouched.
    out.applyAfterSteps = 99;
    out.deferred = false;
    StageId stale;
    stale.crc = StringCRC("stale");
    out.order.Add(stale);
    ConfigError err;

    EXPECT_FALSE(LoadStepChainConfig(Parse(row.json), out, err)) << row.name;
    ExpectDefault(out);
    EXPECT_TRUE(err.key == row.expectedKey) << row.name << " key was '" << err.key.AsCStr() << "'";
    EXPECT_FALSE(err.reason.IsEmpty()) << row.name;
}

INSTANTIATE_TEST_SUITE_P(StepChainConfig, StepChainConfigFault, ::testing::ValuesIn(kFaults),
    [](const ::testing::TestParamInfo<FaultRow>& info) { return std::string(info.param.name); });

TEST(StepChainConfig, NonObjectRootIsRejected)
{
    StepChainConfig<> out;
    ConfigError err;
    EXPECT_FALSE(LoadStepChainConfig(Parse(R"([1,2])"), out, err));
    ExpectDefault(out);
    EXPECT_FALSE(err.key.IsEmpty());
}

TEST(StepChainConfig, LaterFaultStillLeavesOrderDefault)   // all-or-nothing: valid order, bad days
{
    StepChainConfig<> out;
    ConfigError err;
    EXPECT_FALSE(LoadStepChainConfig(Parse(R"({"order":["weather","ecology"],"apply_after_steps":0})"), out, err));
    ExpectDefault(out);
}

TEST(StepChainConfig, HashCollisionIsRejected)   // AC 3
{
    StepChainConfig<> out;
    ConfigError err;
    EXPECT_FALSE(LoadStepChainConfig(Parse(R"({"order":["weather","ecology"]})"), out, err, &ConstantHash));
    ExpectDefault(out);
    EXPECT_TRUE(err.key == "order");
}

TEST(StepChainConfig, DefaultHashDoesNotCollideOnDistinctIds)
{
    StepChainConfig<> out;
    ConfigError err;
    EXPECT_TRUE(LoadStepChainConfig(Parse(R"({"order":["weather","hydro","ecology"]})"), out, err));
}
