#include <gtest/gtest.h>

#include <DiaCamera3D/Health/CameraRegistryHealth3D.h>
#include <DiaCamera3D/Registry/CameraRegistry3D.h>
#include <DiaObservation/Health/IHealthReporter.h>

using namespace Dia::Camera3D;

TEST(DiaCamera3D_Health, ReporterName_IsCorrect)
{
    CameraRegistry3D reg;
    CameraRegistryHealth3D health(reg);
    EXPECT_EQ(health.GetReporterName(), Dia::Core::StringCRC("dia.camera3d.registry"));
}

TEST(DiaCamera3D_Health, EmptyRegistry_ReportsDegraded)
{
    CameraRegistry3D reg;
    CameraRegistryHealth3D health(reg);

    const Dia::Observation::Health::Health h = health.Report();
    EXPECT_EQ(h.status, Dia::Observation::Health::HealthStatus::kDegraded);
    EXPECT_EQ(h.warnings, 1u);
    EXPECT_EQ(h.errors, 0u);
}

TEST(DiaCamera3D_Health, CamerasRegisteredButNoActive_ReportsDegraded)
{
    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("main"), Camera3D{});
    CameraRegistryHealth3D health(reg);

    const Dia::Observation::Health::Health h = health.Report();
    EXPECT_EQ(h.status, Dia::Observation::Health::HealthStatus::kDegraded);
    EXPECT_EQ(h.warnings, 1u);
    EXPECT_EQ(h.errors, 0u);
}

TEST(DiaCamera3D_Health, CameraRegisteredAndActive_ReportsOK)
{
    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("main"), Camera3D{});
    reg.SetActive(Dia::Core::StringCRC("main"));
    CameraRegistryHealth3D health(reg);

    const Dia::Observation::Health::Health h = health.Report();
    EXPECT_EQ(h.status, Dia::Observation::Health::HealthStatus::kOK);
    EXPECT_EQ(h.warnings, 0u);
    EXPECT_EQ(h.errors, 0u);
}

TEST(DiaCamera3D_Health, UnregisterActiveCamera_RevertsToDegraded)
{
    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("main"), Camera3D{});
    reg.SetActive(Dia::Core::StringCRC("main"));

    CameraRegistryHealth3D health(reg);
    EXPECT_EQ(health.Report().status, Dia::Observation::Health::HealthStatus::kOK);

    reg.Unregister(Dia::Core::StringCRC("main"));
    const Dia::Observation::Health::Health h = health.Report();
    EXPECT_EQ(h.status, Dia::Observation::Health::HealthStatus::kDegraded);
}
