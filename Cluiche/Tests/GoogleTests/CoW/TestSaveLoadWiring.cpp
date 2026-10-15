// Wiring tests: SaveLoadModule injects its registry into DiaSimTimeModule (when present)
// so the world clock + scheduler persist, and stays a silent no-op when there is none.
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaSimTime/DiaSimTimeModule.h>
#include <Modules/SaveLoadModule.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using CoW::SaveLoadModule;
using Dia::SimTime::DiaSimTimeModule;

namespace {

SaveLoadModule* g_saveLoad = nullptr;
Module* CreateSaveLoad(const StringCRC& id) { g_saveLoad = new SaveLoadModule(id); return g_saveLoad; }
Module* CreateSimTime(const StringCRC& id) { return new DiaSimTimeModule(id); }

constexpr float kDt = 1.0f / 30.0f;

struct WiringFixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    std::string dir;
    std::string configJson;

    // simTimeFirst puts DiaSimTime BEFORE SaveLoadModule in manifest order to prove injection
    // does not depend on declaration order.
    WiringFixture(const char* name, bool withSimTime, bool simTimeFirst = true)
    {
        dir = std::string("out/test_saves/") + name + "/";
        std::filesystem::remove_all(dir);
        reg.Register(SaveLoadModule::kTypeId, &CreateSaveLoad);
        reg.Register(DiaSimTimeModule::kTypeId, &CreateSimTime);

        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");

        if (withSimTime)
        {
            StreamDeclaration simTime;
            simTime.id = StringCRC("SimTime");
            simTime.kind = StringCRC("FrameStream");
            manifest.streams.Add(simTime);
            StreamDeclaration fire;
            fire.id = StringCRC("SimTimeSchedulerFire");
            fire.kind = StringCRC("EventStream");
            manifest.streams.Add(fire);
        }

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;

        configJson = std::string("{\"base_directory\":\"") + dir + "\"}";
        ModuleDeclaration st;
        st.instanceId = DiaSimTimeModule::kTypeId;
        st.typeId = DiaSimTimeModule::kTypeId;
        st.stages.Add(StringCRC("all"));
        ModuleDeclaration sl;
        sl.instanceId = SaveLoadModule::kTypeId;
        sl.typeId = SaveLoadModule::kTypeId;
        sl.stages.Add(StringCRC("all"));
        sl.configJson = configJson.c_str();

        if (withSimTime && simTimeFirst) pu.modules.Add(st);
        pu.modules.Add(sl);
        if (withSimTime && !simTimeFirst) pu.modules.Add(st);
        manifest.processingUnits.Add(pu);
    }

    ~WiringFixture()
    {
        if (app)
        {
            app->RequestShutdown();
            for (int i = 0; i < 50 && app->Update(kDt); ++i) {}
        }
        app.reset();
        std::filesystem::remove_all(dir);
    }

    bool Boot()
    {
        g_saveLoad = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start() || !g_saveLoad)
            return false;
        for (int i = 0; i < 30; ++i)
        {
            app->Update(kDt);
            if (g_saveLoad->GetState() == ModuleState::kActive)
                return true;
        }
        return false;
    }

    std::string SaveAndRead()
    {
        g_saveLoad->RequestSave(StringCRC("wiring"));
        for (int i = 0; i < 5 && g_saveLoad->GetSaveCount() == 0; ++i)
            app->Update(kDt);
        std::ifstream f(dir + "slot_wiring.sav");
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
};

} // namespace

TEST(SaveLoadWiring, SimTimeParticipantIsInSaveWhenModulePresent)
{
    WiringFixture f("wiring_with", true, true);
    ASSERT_TRUE(f.Boot());
    const std::string json = f.SaveAndRead();
    ASSERT_EQ(g_saveLoad->GetSaveCount(), 1u);
    EXPECT_NE(json.find("\"DiaSimTime\""), std::string::npos) << json;
}

TEST(SaveLoadWiring, InjectionIndependentOfManifestOrder)
{
    WiringFixture f("wiring_order", true, false);
    ASSERT_TRUE(f.Boot());
    const std::string json = f.SaveAndRead();
    ASSERT_EQ(g_saveLoad->GetSaveCount(), 1u);
    EXPECT_NE(json.find("\"DiaSimTime\""), std::string::npos) << json;
}

TEST(SaveLoadWiring, NoSimTimeModuleIsANoOp)
{
    WiringFixture f("wiring_without", false);
    ASSERT_TRUE(f.Boot());
    const std::string json = f.SaveAndRead();
    EXPECT_EQ(g_saveLoad->GetSaveCount(), 1u);
    EXPECT_EQ(json.find("\"DiaSimTime\""), std::string::npos);
}
