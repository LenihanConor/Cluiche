////////////////////////////////////////////////////////////////////////////////
// Filename: TestMetricCommand.cpp
// GoogleTest suite — dia.automation.get_metric command (AC1-AC4)
//
// Spec: docs/specs/features/dia/diaautomation/metric-assertions.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Histogram.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Observation::Metric;

// ---------------------------------------------------------------------------
// Fixture module
// ---------------------------------------------------------------------------
struct MC_SimpleModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;
    StartResult DoStart() override { return StartResult::kReady; }
    void        DoUpdate(float) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC MC_SimpleModule::kTypeId("MC_SimpleModule");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ApplicationManifestV3 McSingleStageManifest()
{
    ApplicationManifestV3 manifest;
    manifest.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("bootMod");
    mod.typeId         = MC_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);
    return manifest;
}

static TypeRegistry McBuildRegistry()
{
    TypeRegistry reg;
    reg.Register(MC_SimpleModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new MC_SimpleModule(id); });
    return reg;
}

static void McShutdown(Application& app)
{
    app.RequestShutdown();
    for (int i = 0; i < 100 && app.Update(1.0f / 60.0f); ++i) {}
}

// ---------------------------------------------------------------------------
// Fixture: reset DiaAPI and MetricRegistry before/after each test
// ---------------------------------------------------------------------------
class MetricCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Dia::API::Shutdown();
        MetricRegistry::Instance().Reset();
    }
    void TearDown() override
    {
        Dia::API::Shutdown();
        MetricRegistry::Instance().Reset();
    }
};

// ===========================================================================
// AC1 — Counter metric: returns {type:"counter", value:<uint>}
// ===========================================================================
TEST_F(MetricCommandTest, GetMetric_Counter_ReturnsValue)
{
    TypeRegistry reg = McBuildRegistry();
    ApplicationManifestV3 manifest = McSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    app.Update(1.0f / 60.0f);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    Counter* counter = MetricRegistry::Instance().RegisterCounter(StringCRC("test.counter"));
    counter->Inc(42);

    Json::Value params(Json::objectValue);
    params["name"] = "test.counter";
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.get_metric"), params);

    ASSERT_TRUE(response["success"].asBool())
        << "Command failed: " << response.toStyledString();

    const Json::Value& data = response["data"];
    EXPECT_EQ(data["type"].asString(), std::string("counter"));
    EXPECT_EQ(data["value"].asUInt64(), static_cast<Json::UInt64>(42));

    McShutdown(app);
}

// ===========================================================================
// AC1 — Gauge metric: returns {type:"gauge", value:<double>}
// ===========================================================================
TEST_F(MetricCommandTest, GetMetric_Gauge_ReturnsValue)
{
    TypeRegistry reg = McBuildRegistry();
    ApplicationManifestV3 manifest = McSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    app.Update(1.0f / 60.0f);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    Gauge* gauge = MetricRegistry::Instance().RegisterGauge(StringCRC("test.gauge"));
    gauge->Set(3.14);

    Json::Value params(Json::objectValue);
    params["name"] = "test.gauge";
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.get_metric"), params);

    ASSERT_TRUE(response["success"].asBool())
        << "Command failed: " << response.toStyledString();

    const Json::Value& data = response["data"];
    EXPECT_EQ(data["type"].asString(), std::string("gauge"));
    EXPECT_NEAR(data["value"].asDouble(), 3.14, 1e-9);

    McShutdown(app);
}

// ===========================================================================
// AC2 — Unknown metric returns {success:false, error:"metric not found: ..."}
// ===========================================================================
TEST_F(MetricCommandTest, GetMetric_Unknown_ReturnsError)
{
    TypeRegistry reg = McBuildRegistry();
    ApplicationManifestV3 manifest = McSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    app.Update(1.0f / 60.0f);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    Json::Value params(Json::objectValue);
    params["name"] = "does.not.exist";
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.get_metric"), params);

    EXPECT_FALSE(response["success"].asBool());
    const std::string error = response["error"].asString();
    EXPECT_NE(error.find("metric not found"), std::string::npos)
        << "Error message was: " << error;
    EXPECT_NE(error.find("does.not.exist"), std::string::npos)
        << "Error message should include the metric name, was: " << error;

    McShutdown(app);
}

// ===========================================================================
// AC3 — Live value: counter updated after command registration; query returns
//        the current value, not a cached one.
// ===========================================================================
TEST_F(MetricCommandTest, GetMetric_Counter_ReturnsLiveValue)
{
    TypeRegistry reg = McBuildRegistry();
    ApplicationManifestV3 manifest = McSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    app.Update(1.0f / 60.0f);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    Counter* counter = MetricRegistry::Instance().RegisterCounter(StringCRC("test.live"));
    counter->Inc(10);

    Json::Value params(Json::objectValue);
    params["name"] = "test.live";

    Json::Value r1 = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.get_metric"), params);
    ASSERT_TRUE(r1["success"].asBool());
    EXPECT_EQ(r1["data"]["value"].asUInt64(), static_cast<Json::UInt64>(10));

    counter->Inc(5);

    Json::Value r2 = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.get_metric"), params);
    ASSERT_TRUE(r2["success"].asBool());
    EXPECT_EQ(r2["data"]["value"].asUInt64(), static_cast<Json::UInt64>(15));

    McShutdown(app);
}

// ===========================================================================
// AC4 — Histogram: returns {type:"histogram", value:{count,sum,min,max,mean}}
// ===========================================================================
TEST_F(MetricCommandTest, GetMetric_Histogram_ReturnsSnapshot)
{
    TypeRegistry reg = McBuildRegistry();
    ApplicationManifestV3 manifest = McSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    app.Update(1.0f / 60.0f);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    float bounds[] = { 10.0f, 20.0f, 50.0f };
    Histogram* hist = MetricRegistry::Instance().RegisterHistogram(
        StringCRC("test.histogram"), bounds, 3);
    hist->Observe(5.0);
    hist->Observe(15.0);
    hist->Observe(30.0);

    Json::Value params(Json::objectValue);
    params["name"] = "test.histogram";
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.get_metric"), params);

    ASSERT_TRUE(response["success"].asBool())
        << "Command failed: " << response.toStyledString();

    const Json::Value& data  = response["data"];
    const Json::Value& value = data["value"];

    EXPECT_EQ(data["type"].asString(), std::string("histogram"));
    EXPECT_EQ(value["count"].asUInt64(), static_cast<Json::UInt64>(3));
    EXPECT_NEAR(value["sum"].asDouble(), 50.0, 1e-9);
    EXPECT_TRUE(value.isMember("min"));
    EXPECT_TRUE(value.isMember("max"));
    EXPECT_NEAR(value["mean"].asDouble(), 50.0 / 3.0, 1e-6);

    McShutdown(app);
}

// ===========================================================================
// AC1 — Missing 'name' parameter returns {success:false, error:...}
// ===========================================================================
TEST_F(MetricCommandTest, GetMetric_MissingParam_ReturnsError)
{
    TypeRegistry reg = McBuildRegistry();
    ApplicationManifestV3 manifest = McSingleStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    app.Update(1.0f / 60.0f);

    Dia::Automation::AutomationService service(app);
    service.RegisterCommands();

    Json::Value params(Json::objectValue); // no "name" key
    Json::Value response = Dia::API::ExecuteCommandJson(StringCRC("dia.automation.get_metric"), params);

    EXPECT_FALSE(response["success"].asBool());
    EXPECT_FALSE(response["error"].asString().empty());

    McShutdown(app);
}
