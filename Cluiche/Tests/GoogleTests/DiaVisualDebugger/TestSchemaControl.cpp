#include <gtest/gtest.h>
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/SchemaControl.h>
#include <DiaVisualDebugger/Domain/ISchemaKeyResolver.h>
#include <DiaObservation/Testing/MockSink.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <cstring>

using namespace Dia::VisualDebugger;
using Dia::Core::StringCRC;

namespace
{
    struct FakeResolver : public ISchemaKeyResolver
    {
        StringCRC     known = StringCRC("cow.ecology_params/grazing.rate");
        unsigned int  version = 7;
        bool Resolves(StringCRC key) const override { return key == known; }
        unsigned int Version() const override { return version; }
    };

    struct ScopedSink
    {
        Dia::Observation::Log::MockSink sink;
        ScopedSink()
        {
            Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
            Dia::Observation::Log::Logger::Instance().RegisterSink(&sink);
        }
        ~ScopedSink()
        {
            Dia::Observation::Log::Logger::Instance().UnregisterSink(&sink);
            Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
        }
        unsigned int Warnings() const
        {
            Dia::Observation::Log::Logger::Instance().FlushSync();
            unsigned int n = 0;
            for (unsigned int i = 0; i < sink.Count(); ++i)
            {
                if (sink.entries[i].entry.level == Dia::Observation::Log::LogLevel::kWarning &&
                    sink.entries[i].entry.channel == StringCRC("VSC"))
                {
                    ++n;
                }
            }
            return n;
        }
    };

    unsigned long long UnresolvedMetric()
    {
        auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC("valueschema.panel_unresolved_keys"));
        return c ? c->Value() : 0;
    }
}

TEST(DiaVisualDebugger_SchemaControl, Add_EmitsKeyNameValueAndDefaultSliderType)
{
    SchemaControl sc;
    Json::Value controls(Json::arrayValue);
    sc.Add(controls, "grazing.rate", StringCRC("cow.ecology_params/grazing.rate"), 0.8f);
    ASSERT_EQ(1u, controls.size());
    EXPECT_STREQ("grazing.rate", controls[0u]["name"].asCString());
    EXPECT_STREQ("slider", controls[0u]["type"].asCString());
    EXPECT_FLOAT_EQ(0.8f, controls[0u]["value"].asFloat());
    EXPECT_STREQ("cow.ecology_params/grazing.rate", controls[0u]["schemaKey"].asCString());
    EXPECT_FALSE(controls[0u].isMember("packId"));
}

TEST(DiaVisualDebugger_SchemaControl, Add_PackIdOnlyWhenGiven)
{
    SchemaControl sc;
    Json::Value controls(Json::arrayValue);
    sc.Add(controls, "a", StringCRC("k/a"), 1.0f, "lush");
    sc.Add(controls, "b", StringCRC("k/b"), 1.0f, "");
    EXPECT_STREQ("lush", controls[0u]["packId"].asCString());
    EXPECT_FALSE(controls[1u].isMember("packId"));
}

TEST(DiaVisualDebugger_SchemaControl, Add_CustomType)
{
    SchemaControl sc;
    Json::Value controls(Json::arrayValue);
    sc.Add(controls, "a", StringCRC("k/a"), 1.0f, nullptr, "number");
    EXPECT_STREQ("number", controls[0u]["type"].asCString());
}

TEST(DiaVisualDebugger_SchemaControl, NoResolver_EmitsKeyNoWarningNoCheck)
{
    ScopedSink mock;
    SchemaControl sc;
    Json::Value controls(Json::arrayValue);
    sc.Add(controls, "x", StringCRC("nope/missing"), 1.0f);
    EXPECT_TRUE(controls[0u].isMember("schemaKey"));
    EXPECT_EQ(0u, mock.Warnings());
    EXPECT_EQ(0u, sc.UnresolvedCount());
}

TEST(DiaVisualDebugger_SchemaControl, ResolvedKey_NoWarning)
{
    ScopedSink mock;
    FakeResolver r;
    SchemaControl sc(&r);
    Json::Value controls(Json::arrayValue);
    sc.Add(controls, "grazing.rate", StringCRC("cow.ecology_params/grazing.rate"), 0.8f);
    EXPECT_EQ(0u, mock.Warnings());
    EXPECT_EQ(0u, sc.UnresolvedCount());
}

TEST(DiaVisualDebugger_SchemaControl, UnresolvedKey_WarnsOncePerKey_AndCountsMetric)
{
    ScopedSink mock;
    FakeResolver r;
    SchemaControl sc(&r);
    const unsigned long long before = UnresolvedMetric();
    Json::Value controls(Json::arrayValue);
    for (int i = 0; i < 3; ++i) sc.Add(controls, "x", StringCRC("bad/one"), 1.0f);
    EXPECT_EQ(1u, mock.Warnings());
    sc.Add(controls, "y", StringCRC("bad/two"), 1.0f);
    sc.Add(controls, "y", StringCRC("bad/two"), 1.0f);
    EXPECT_EQ(2u, mock.Warnings());
    EXPECT_EQ(2u, sc.UnresolvedCount());
    EXPECT_EQ(before + 2, UnresolvedMetric());
    EXPECT_EQ(5u, controls.size());
}

TEST(DiaVisualDebugger_SchemaControl, SeenKeyCap_OverflowCounted)
{
    FakeResolver r;
    SchemaControl sc(&r);
    Json::Value controls(Json::arrayValue);
    for (unsigned int i = 0; i < SchemaControl::kMaxSeenKeys + 3; ++i)
    {
        char key[32];
        snprintf(key, sizeof(key), "bad/k%u", i);
        sc.Add(controls, "x", StringCRC(key), 1.0f);
    }
    EXPECT_EQ(3u, sc.OverflowCount());
    EXPECT_EQ(SchemaControl::kMaxSeenKeys, sc.UnresolvedCount());
}

TEST(DiaVisualDebugger_SchemaControl, AddSchemaVersion_OnlyWithResolver)
{
    Json::Value none(Json::objectValue);
    SchemaControl().AddSchemaVersion(none);
    EXPECT_FALSE(none.isMember("schemaVersion"));

    FakeResolver r;
    SchemaControl sc(&r);
    Json::Value state(Json::objectValue);
    sc.AddSchemaVersion(state);
    EXPECT_EQ(7u, state["schemaVersion"].asUInt());
}

#endif
