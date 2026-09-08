#include <gtest/gtest.h>
#include <DiaObservation/Testing/HealthFixture.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Observation::Health;
using namespace Dia::Observation::Testing;

struct HealthReporterBaseTest : ::testing::Test
{
    HealthFixture fixture;
    MockHealthReporter reporter{Dia::Core::StringCRC("TestReporter")};
};

TEST_F(HealthReporterBaseTest, DefaultStatus_IsOK)
{
    Health h = reporter.Report();
    EXPECT_EQ(h.status, HealthStatus::kOK);
    EXPECT_EQ(h.errors, 0u);
    EXPECT_EQ(h.warnings, 0u);
}

TEST_F(HealthReporterBaseTest, SetFailing_ReportsCorrectly)
{
    reporter.SetFailing(Dia::Core::StringCRC("disk_full"));
    Health h = reporter.Report();
    EXPECT_EQ(h.status, HealthStatus::kFailing);
    EXPECT_EQ(h.reason.Value(), Dia::Core::StringCRC("disk_full").Value());
}

TEST_F(HealthReporterBaseTest, SetDegraded_ReportsCorrectly)
{
    reporter.SetDegraded(Dia::Core::StringCRC("high_latency"));
    Health h = reporter.Report();
    EXPECT_EQ(h.status, HealthStatus::kDegraded);
}

TEST_F(HealthReporterBaseTest, SetOK_ClearsReasonAndStatus)
{
    reporter.SetFailing(Dia::Core::StringCRC("err"));
    reporter.SetOK();
    Health h = reporter.Report();
    EXPECT_EQ(h.status, HealthStatus::kOK);
}

TEST_F(HealthReporterBaseTest, IncrementErrors_Accumulates)
{
    reporter.IncrementErrors();
    reporter.IncrementErrors();
    reporter.IncrementErrors();
    Health h = reporter.Report();
    EXPECT_EQ(h.errors, 3u);
}

TEST_F(HealthReporterBaseTest, IncrementWarnings_Accumulates)
{
    reporter.IncrementWarnings();
    reporter.IncrementWarnings();
    Health h = reporter.Report();
    EXPECT_EQ(h.warnings, 2u);
}

TEST_F(HealthReporterBaseTest, GetReporterName_ReturnsConfiguredName)
{
    EXPECT_EQ(reporter.GetReporterName().Value(),
              Dia::Core::StringCRC("TestReporter").Value());
}
