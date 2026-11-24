#include <gtest/gtest.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/MetricInfo.h>

#include <cstring>

using namespace Dia::Observation::Metric;
using Dia::Core::StringCRC;

namespace
{
    struct Seen
    {
        bool         found = false;
        int          kind  = -1;
        MetricInfo   info;
        bool         hasInfo = false;
        StringCRC    target;
    };

    void Collect(const StringCRC& name, MetricKind kind, const MetricInfo* info, void* user)
    {
        Seen* s = static_cast<Seen*>(user);
        if (name == s->target)
        {
            s->found   = true;
            s->kind    = static_cast<int>(kind);
            s->hasInfo = (info != nullptr);
            if (info) s->info = *info;
        }
    }

    Seen Find(const StringCRC& name)
    {
        Seen s;
        s.target = name;
        MetricRegistry::Instance().ForEachMetric(&Collect, &s);
        return s;
    }

    const float kBounds[] = { 1.0f, 2.0f, 4.0f };
}

TEST(MetricInfoTest, CounterRoundTrip)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC name("metricinfo.test.1");
    MetricInfo info{ "Label", "Desc", "ms" };
    Counter* c = r.RegisterCounter(name, info);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c, r.FindCounter(name));
    const MetricInfo* got = r.GetInfo(name);
    ASSERT_NE(got, nullptr);
    EXPECT_STREQ(got->label, "Label");
    EXPECT_STREQ(got->description, "Desc");
    EXPECT_STREQ(got->unit, "ms");
}

TEST(MetricInfoTest, GaugeRoundTrip)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC name("metricinfo.test.2");
    Gauge* g = r.RegisterGauge(name, MetricInfo{ "G", "GD", "bytes" });
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g, r.FindGauge(name));
    const MetricInfo* got = r.GetInfo(name);
    ASSERT_NE(got, nullptr);
    EXPECT_STREQ(got->label, "G");
    EXPECT_STREQ(got->unit, "bytes");
}

TEST(MetricInfoTest, HistogramRoundTrip)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC name("metricinfo.test.3");
    Histogram* h = r.RegisterHistogram(name, kBounds, 3, MetricInfo{ "H", "HD", "s" });
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h, r.FindHistogram(name));
    const MetricInfo* got = r.GetInfo(name);
    ASSERT_NE(got, nullptr);
    EXPECT_STREQ(got->description, "HD");
}

TEST(MetricInfoTest, OriginalOverloadsUnaffected)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC c("metricinfo.test.4c"), g("metricinfo.test.4g"), h("metricinfo.test.4h");
    Counter* pc = r.RegisterCounter(c);
    EXPECT_NE(pc, nullptr);
    EXPECT_NE(r.RegisterGauge(g), nullptr);
    EXPECT_NE(r.RegisterHistogram(h, kBounds, 3), nullptr);
    EXPECT_EQ(r.RegisterCounter(c), pc);
    EXPECT_EQ(r.GetInfo(c), nullptr);
    EXPECT_EQ(r.GetInfo(g), nullptr);
    EXPECT_EQ(r.GetInfo(h), nullptr);
}

TEST(MetricInfoTest, GetInfoNullForUnknown)
{
    EXPECT_EQ(MetricRegistry::Instance().GetInfo(StringCRC("metricinfo.test.unknown")), nullptr);
}

TEST(MetricInfoTest, ReRegisterReturnsSameObjectWithInfo)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC name("metricinfo.test.5");
    Counter* a = r.RegisterCounter(name, MetricInfo{ "L", "D", "u" });
    Counter* b = r.RegisterCounter(name, MetricInfo{ "L", "D", "u" });
    Counter* c = r.RegisterCounter(name);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, c);
}

TEST(MetricInfoTest, ForEachMetricListsNameKindInfo)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC c("metricinfo.test.6c"), g("metricinfo.test.6g"), h("metricinfo.test.6h"), n("metricinfo.test.6n");
    r.RegisterCounter(c, MetricInfo{ "CL", "CD", "c" });
    r.RegisterGauge(g, MetricInfo{ "GL", "GD", "g" });
    r.RegisterHistogram(h, kBounds, 3, MetricInfo{ "HL", "HD", "h" });
    r.RegisterCounter(n);

    Seen sc = Find(c);
    ASSERT_TRUE(sc.found);
    EXPECT_EQ(sc.kind, static_cast<int>(MetricKind::Counter));
    ASSERT_TRUE(sc.hasInfo);
    EXPECT_STREQ(sc.info.label, "CL");

    Seen sg = Find(g);
    ASSERT_TRUE(sg.found);
    EXPECT_EQ(sg.kind, static_cast<int>(MetricKind::Gauge));
    EXPECT_STREQ(sg.info.unit, "g");

    Seen sh = Find(h);
    ASSERT_TRUE(sh.found);
    EXPECT_EQ(sh.kind, static_cast<int>(MetricKind::Histogram));
    EXPECT_STREQ(sh.info.description, "HD");

    Seen sn = Find(n);
    ASSERT_TRUE(sn.found);
    EXPECT_FALSE(sn.hasInfo);
}

TEST(MetricInfoTest, GenerationIncrementsOnNewMetricAndInfo)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC a("metricinfo.test.7a"), b("metricinfo.test.7b");

    unsigned int g0 = r.Generation();
    r.RegisterCounter(a);                       // new metric
    unsigned int g1 = r.Generation();
    EXPECT_GT(g1, g0);

    r.RegisterCounter(a);                       // pure lookup, no info
    EXPECT_EQ(r.Generation(), g1);

    r.RegisterCounter(a, MetricInfo{ "L", "D", "u" }); // stores info on existing metric
    unsigned int g2 = r.Generation();
    EXPECT_GT(g2, g1);

    r.RegisterCounter(a, MetricInfo{ "L", "D", "u" }); // same info: lookup
    EXPECT_EQ(r.Generation(), g2);

    r.RegisterCounter(a);                       // no info after info: lookup
    EXPECT_EQ(r.Generation(), g2);

    r.RegisterGauge(b, MetricInfo{ "L", "D", "u" }); // new metric with info
    EXPECT_GT(r.Generation(), g2);
}

TEST(MetricInfoTest, ConflictingInfoKeepsFirstAndWarnsOncePerName)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC name("metricinfo.test.8");
    r.RegisterCounter(name, MetricInfo{ "First", "D", "u" });

    unsigned int w0 = r.ConflictWarningCount();
    unsigned int g0 = r.Generation();
    r.RegisterCounter(name, MetricInfo{ "Second", "D", "u" });
    EXPECT_EQ(r.ConflictWarningCount(), w0 + 1);
    r.RegisterCounter(name, MetricInfo{ "Third", "D2", "u" });
    EXPECT_EQ(r.ConflictWarningCount(), w0 + 1);

    EXPECT_STREQ(r.GetInfo(name)->label, "First");
    EXPECT_EQ(r.Generation(), g0);
}

TEST(MetricInfoTest, ConflictComparesByContentNotPointer)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC name("metricinfo.test.9");
    char buf1[] = "same";
    char buf2[] = "same";
    r.RegisterCounter(name, MetricInfo{ buf1, "D", "u" });
    unsigned int w0 = r.ConflictWarningCount();
    r.RegisterCounter(name, MetricInfo{ buf2, "D", "u" });
    EXPECT_EQ(r.ConflictWarningCount(), w0);
    r.RegisterCounter(name, MetricInfo{ buf2, "D", "different" });
    EXPECT_EQ(r.ConflictWarningCount(), w0 + 1);
}

TEST(MetricInfoTest, NoInfoAfterInfoKeepsInfo)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC name("metricinfo.test.10");
    r.RegisterGauge(name, MetricInfo{ "Keep", "D", "u" });
    unsigned int w0 = r.ConflictWarningCount();
    r.RegisterGauge(name);
    ASSERT_NE(r.GetInfo(name), nullptr);
    EXPECT_STREQ(r.GetInfo(name)->label, "Keep");
    EXPECT_EQ(r.ConflictWarningCount(), w0);
}

TEST(MetricInfoTest, InfoAfterNoInfoStoresInfo)
{
    MetricRegistry& r = MetricRegistry::Instance();
    StringCRC name("metricinfo.test.11");
    r.RegisterHistogram(name, kBounds, 3);
    EXPECT_EQ(r.GetInfo(name), nullptr);
    unsigned int w0 = r.ConflictWarningCount();
    r.RegisterHistogram(name, kBounds, 3, MetricInfo{ "Late", "D", "u" });
    ASSERT_NE(r.GetInfo(name), nullptr);
    EXPECT_STREQ(r.GetInfo(name)->label, "Late");
    EXPECT_EQ(r.ConflictWarningCount(), w0);
}
