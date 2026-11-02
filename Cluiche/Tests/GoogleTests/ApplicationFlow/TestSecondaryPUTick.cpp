////////////////////////////////////////////////////////////////////////////////
// Filename: TestSecondaryPUTick.cpp
// GoogleTest suite — a non-dedicated secondary ProcessingUnit runs inline.
//
// `dedicatedThread = false` means "run on the caller's thread". Application::
// Update must therefore tick every non-dedicated PU (not only PU index 0), in
// manifest order, so a two-PU application can be stepped deterministically.
// Dedicated-thread PUs are not ticked by Update (they own a thread).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

namespace {

    struct CountingModule : Module
    {
        using Module::Module;
        static const StringCRC kTypeId;
        int updateCalls = 0;
        StartResult DoStart() override { return StartResult::kReady; }
        void        DoUpdate(float) override { ++updateCalls; }
        StopResult  DoStop()  override { return StopResult::kDone; }
    };
    const StringCRC CountingModule::kTypeId("SecondaryPUTick_CountingModule");

    CountingModule* gFirst = nullptr;
    CountingModule* gSecond = nullptr;

    ProcessingUnitDeclaration MakePU(const char* puId, const char* moduleId, bool dedicated)
    {
        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC(puId);
        pu.frequencyHz     = 30.0f;
        pu.dedicatedThread = dedicated;

        ModuleDeclaration mod;
        mod.instanceId     = StringCRC(moduleId);
        mod.typeId         = CountingModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
        return pu;
    }

    ApplicationManifestV3 TwoPuManifest(bool secondDedicated)
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;
        StageDeclaration stage;
        stage.name = StringCRC("Boot");
        manifest.stages.Add(stage);
        manifest.initialStage = StringCRC("Boot");
        manifest.processingUnits.Add(MakePU("FirstPU", "first", false));
        manifest.processingUnits.Add(MakePU("SecondPU", "second", secondDedicated));
        return manifest;
    }

    void RegisterCounting(TypeRegistry& reg)
    {
        gFirst = nullptr;
        gSecond = nullptr;
        reg.Register(CountingModule::kTypeId, [](const StringCRC& id) -> Module*
        {
            auto* m = new CountingModule(id);
            if (id == StringCRC("first")) gFirst = m; else gSecond = m;
            return m;
        });
    }

    constexpr float kDt = 0.0334f; // just over one 30 Hz step
}

TEST(SecondaryPUTick, NonDedicatedSecondPuModulesAreStartedAndUpdatedByApplicationUpdate)
{
    TypeRegistry reg;
    RegisterCounting(reg);
    Application app(TwoPuManifest(false), reg);
    ASSERT_TRUE(app.Start());

    for (int i = 0; i < 10; ++i) app.Update(kDt);

    ASSERT_NE(gFirst, nullptr);
    ASSERT_NE(gSecond, nullptr);
    EXPECT_EQ(gFirst->GetState(), ModuleState::kActive);
    EXPECT_EQ(gSecond->GetState(), ModuleState::kActive) << "a non-dedicated secondary PU must run inline";
    EXPECT_GT(gFirst->updateCalls, 0);
    EXPECT_GT(gSecond->updateCalls, 0);

    app.RequestShutdown();
    int guard = 0;
    while (app.Update(kDt) && ++guard < 100) {}
    EXPECT_LT(guard, 100) << "shutdown must settle once the secondary PU's modules stop";
}

TEST(SecondaryPUTick, BothPusAdvanceInLockstepOneUpdatePerStep)
{
    TypeRegistry reg;
    RegisterCounting(reg);
    Application app(TwoPuManifest(false), reg);
    ASSERT_TRUE(app.Start());
    for (int i = 0; i < 5; ++i) app.Update(kDt);

    const int firstBefore = gFirst->updateCalls;
    const int secondBefore = gSecond->updateCalls;
    app.Update(kDt);
    EXPECT_EQ(gFirst->updateCalls, firstBefore + 1);
    EXPECT_EQ(gSecond->updateCalls, secondBefore + 1);

    app.RequestShutdown();
    for (int guard = 0; guard < 100 && app.Update(kDt); ++guard) {}
}
