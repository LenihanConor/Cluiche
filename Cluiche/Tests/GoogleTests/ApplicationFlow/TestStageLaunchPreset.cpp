////////////////////////////////////////////////////////////////////////////////
// Filename: TestStageLaunchPreset.cpp
// GoogleTest suite — Module::OnStageLaunch delivery (launch-presets AC 6)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstring>
#include <string>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

namespace
{
    // Shared event log: "launch:<id>:<json>" and "start:<id>" in call order.
    std::vector<std::string>& CallLog()
    {
        static std::vector<std::string> log;
        return log;
    }

    struct LaunchModule : SimModule
    {
        using SimModule::SimModule;
        static const StringCRC kTypeId;
        StartResult DoStart() override
        {
            CallLog().push_back(std::string("start:") + GetInstanceId().AsChar());
            return StartResult::kReady;
        }
        void       DoUpdate(const Dia::SimTime::SimTimeContext&) override {}
        StopResult DoStop() override { return StopResult::kDone; }
        void OnStageLaunch(const char* json) override
        {
            CallLog().push_back(std::string("launch:") + GetInstanceId().AsChar() + ":" + (json ? json : "<null>"));
        }
    };
    const StringCRC LaunchModule::kTypeId("LaunchModule");

    void AddModule(ProcessingUnitDeclaration& pu, const char* id, const char* stage)
    {
        ModuleDeclaration mod;
        mod.instanceId     = StringCRC(id);
        mod.typeId         = LaunchModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC(stage));
        pu.modules.Add(mod);
    }

    // Appends one override to a preset, text stored in the manifest pool.
    void AddOverride(ApplicationManifestV3& m, PresetDeclaration& p, const char* moduleId, const char* json)
    {
        PresetModuleOverride& o = p.modules[p.moduleCount++];
        o.instanceId = StringCRC(moduleId);
        o.offset     = m.presetOverridePoolUsed;
        o.length     = static_cast<unsigned int>(std::strlen(json));
        std::memcpy(m.presetOverridePool + o.offset, json, o.length);
        m.presetOverridePool[o.offset + o.length] = '\0';
        m.presetOverridePoolUsed += o.length + 1;
    }

    struct Fixture
    {
        TypeRegistry          reg;
        ApplicationManifestV3 manifest;

        // Stages: Boot (initial, modBoot), Game (modG1, modG2, modG3), plus "modAll" in all.
        Fixture()
        {
            CallLog().clear();
            reg.Register(LaunchModule::kTypeId,
                [](const StringCRC& id) -> Module* { return new LaunchModule(id); });

            manifest.version = 3;
            StageDeclaration boot, game;
            boot.name = StringCRC("Boot");
            game.name = StringCRC("Game");
            game.presetFirst = 0;
            game.presetCount = 1;
            manifest.stages.Add(boot);
            manifest.stages.Add(game);
            manifest.initialStage = StringCRC("Boot");

            ProcessingUnitDeclaration pu;
            pu.instanceId      = StringCRC("MainPU");
            pu.frequencyHz     = 60.0f;
            pu.dedicatedThread = false;
            AddModule(pu, "modBoot", "Boot");
            AddModule(pu, "modG1", "Game");
            AddModule(pu, "modG2", "Game");
            AddModule(pu, "modG3", "Game");
            {
                ModuleDeclaration all;
                all.instanceId     = StringCRC("modAll");
                all.typeId         = LaunchModule::kTypeId;
                all.startTimeoutMs = 10000.0f;
                all.stopTimeoutMs  = 5000.0f;
                all.stages.Add(StringCRC("all"));
                pu.modules.Add(all);
            }
            manifest.processingUnits.Add(pu);

            PresetDeclaration preset;
            preset.name = StringCRC("Fast");
            AddOverride(manifest, preset, "modG1", "{\"a\":1}");
            AddOverride(manifest, preset, "modAll", "{\"b\":2}");
            manifest.presets.Add(preset);
        }
    };

    void Pump(Application& app, int frames = 30)
    {
        for (int i = 0; i < frames; ++i)
            app.Update(1.0f / 60.0f);
    }

    int IndexOf(const std::string& s)
    {
        for (size_t i = 0; i < CallLog().size(); ++i)
            if (CallLog()[i] == s) return static_cast<int>(i);
        return -1;
    }
}

TEST(StageLaunchPreset, InitialStageGetsEmptyOverrideBeforeStart)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);

    int launch = IndexOf("launch:modBoot:");
    int start  = IndexOf("start:modBoot");
    ASSERT_GE(launch, 0);
    ASSERT_GE(start, 0);
    EXPECT_LT(launch, start);
    EXPECT_GE(IndexOf("launch:modAll:"), 0);
}

TEST(StageLaunchPreset, OnlyStartingModulesAreLaunchedAndEmptyWithoutPreset)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    CallLog().clear();

    app.TransitionTo(StringCRC("Game"));
    Pump(app);

    EXPECT_GE(IndexOf("launch:modG1:"), 0);
    EXPECT_GE(IndexOf("launch:modG2:"), 0);
    EXPECT_GE(IndexOf("launch:modG3:"), 0);
    EXPECT_LT(IndexOf("launch:modG1:"), IndexOf("start:modG1"));
    EXPECT_EQ(IndexOf("launch:modBoot:"), -1);
    // modAll is already running in "all" scope: not launched again.
    for (const std::string& s : CallLog())
        EXPECT_EQ(s.find("modAll"), std::string::npos) << s;
}

TEST(StageLaunchPreset, PresetDeliversPerModuleOverrides)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    CallLog().clear();

    app.TransitionToInternal(StringCRC("Game"), StringCRC("Fast"));
    Pump(app);

    EXPECT_GE(IndexOf("launch:modG1:{\"a\":1}"), 0);
    EXPECT_LT(IndexOf("launch:modG1:{\"a\":1}"), IndexOf("start:modG1"));
    // In the preset's module list but not starting / present: no call for running modAll.
    for (const std::string& s : CallLog())
        EXPECT_EQ(s.find("modAll"), std::string::npos) << s;
}

TEST(StageLaunchPreset, ModuleWithoutOverrideGetsEmptyString)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    CallLog().clear();

    app.TransitionToInternal(StringCRC("Game"), StringCRC("Fast"));
    Pump(app);

    EXPECT_GE(IndexOf("launch:modG2:"), 0);
    EXPECT_GE(IndexOf("launch:modG3:"), 0);
}

TEST(StageLaunchPreset, PresetDoesNotLeakIntoLaterTransition)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    app.TransitionToInternal(StringCRC("Game"), StringCRC("Fast"));
    Pump(app);
    app.TransitionTo(StringCRC("Boot"));
    Pump(app);
    CallLog().clear();
    app.TransitionTo(StringCRC("Game"));
    Pump(app);

    EXPECT_GE(IndexOf("launch:modG1:"), 0);
    EXPECT_EQ(IndexOf("launch:modG1:{\"a\":1}"), -1);
}

TEST(StageLaunchPreset, UnknownPresetBehavesAsNoPreset)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    CallLog().clear();

    app.TransitionToInternal(StringCRC("Game"), StringCRC("Nope"));
    Pump(app);

    EXPECT_GE(IndexOf("launch:modG1:"), 0);
}

// ---- IApplicationControl preset API (launch-presets AC 5, 7) ----

TEST(StageLaunchPreset, CurrentPresetEmptyAtStart)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    IApplicationControl& ctl = app;
    EXPECT_EQ(ctl.GetCurrentPreset().Value(), 0u);
}

TEST(StageLaunchPreset, ControlTransitionWithValidPresetDeliversAndReportsIt)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    CallLog().clear();

    IApplicationControl& ctl = app;
    ctl.TransitionTo(StringCRC("Game"), StringCRC("Fast"));
    Pump(app);

    EXPECT_EQ(ctl.GetCurrentStage(), StringCRC("Game"));
    EXPECT_EQ(ctl.GetCurrentPreset(), StringCRC("Fast"));
    EXPECT_GE(IndexOf("launch:modG1:{\"a\":1}"), 0);
}

TEST(StageLaunchPreset, ControlTransitionWithUnknownPresetIsRefused)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    CallLog().clear();

    IApplicationControl& ctl = app;
    ctl.TransitionTo(StringCRC("Game"), StringCRC("Nope"));
    Pump(app);

    EXPECT_EQ(ctl.GetCurrentStage(), StringCRC("Boot"));
    EXPECT_EQ(ctl.GetCurrentPreset().Value(), 0u);
    EXPECT_EQ(IndexOf("launch:modG1:"), -1);
    EXPECT_EQ(IndexOf("start:modG1"), -1);
}

TEST(StageLaunchPreset, UnknownPresetKeepsPreviousCurrentPreset)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    IApplicationControl& ctl = app;
    ctl.TransitionTo(StringCRC("Game"), StringCRC("Fast"));
    Pump(app);
    ctl.TransitionTo(StringCRC("Boot"), StringCRC("Nope"));
    Pump(app);

    EXPECT_EQ(ctl.GetCurrentStage(), StringCRC("Game"));
    EXPECT_EQ(ctl.GetCurrentPreset(), StringCRC("Fast"));
}

TEST(StageLaunchPreset, PresetOfOtherStageIsRefused)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    IApplicationControl& ctl = app;
    ctl.TransitionTo(StringCRC("Boot"), StringCRC("Fast"));   // Fast belongs to Game
    ctl.TransitionTo(StringCRC("Game"), StringCRC("Fast"));
    Pump(app);
    ctl.TransitionTo(StringCRC("Boot"), StringCRC("Fast"));
    Pump(app);

    EXPECT_EQ(ctl.GetCurrentStage(), StringCRC("Game"));
}

TEST(StageLaunchPreset, PlainTransitionClearsCurrentPreset)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    IApplicationControl& ctl = app;
    ctl.TransitionTo(StringCRC("Game"), StringCRC("Fast"));
    Pump(app);
    ASSERT_EQ(ctl.GetCurrentPreset(), StringCRC("Fast"));
    ctl.TransitionTo(StringCRC("Boot"));
    Pump(app);

    EXPECT_EQ(ctl.GetCurrentStage(), StringCRC("Boot"));
    EXPECT_EQ(ctl.GetCurrentPreset().Value(), 0u);
}

TEST(StageLaunchPreset, EmptyPresetBehavesLikePlainTransition)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    ASSERT_TRUE(app.Start());
    Pump(app);
    CallLog().clear();
    IApplicationControl& ctl = app;
    ctl.TransitionTo(StringCRC("Game"), StringCRC());
    Pump(app);

    EXPECT_EQ(ctl.GetCurrentStage(), StringCRC("Game"));
    EXPECT_EQ(ctl.GetCurrentPreset().Value(), 0u);
    EXPECT_GE(IndexOf("launch:modG1:"), 0);
}

// ---- Launch presets AC 13: initial launch hand-off ----

TEST(InitialLaunch, NoneSetByDefault)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    StringCRC stage, preset;
    EXPECT_FALSE(app.TakeInitialLaunch(stage, preset));
}

TEST(InitialLaunch, TakenExactlyOnce)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    app.SetInitialLaunch(StringCRC("Game"), StringCRC("Fast"));
    StringCRC stage, preset;
    ASSERT_TRUE(app.TakeInitialLaunch(stage, preset));
    EXPECT_EQ(stage, StringCRC("Game"));
    EXPECT_EQ(preset, StringCRC("Fast"));
    EXPECT_FALSE(app.TakeInitialLaunch(stage, preset));
}

TEST(InitialLaunch, EmptyStageIsNotALaunch)
{
    Fixture f;
    Application app(f.manifest, f.reg);
    app.SetInitialLaunch(StringCRC(), StringCRC("Fast"));
    StringCRC stage, preset;
    EXPECT_FALSE(app.TakeInitialLaunch(stage, preset));
}
