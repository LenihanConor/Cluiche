// TestLiveStateStore.cpp - Unit tests for LiveStateStore (DiaApplicationFlowInspector)
//
// Validates push-based runtime state management for the live overlay inspector plugin.

#include <gtest/gtest.h>
#include <DiaApplicationFlowInspector/LiveStateStore.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::Core;

// ==============================================================================
// Helpers
// ==============================================================================

static LiveAppState MakeAppState(const char* stage,
                                  bool transitioning = false,
                                  const char* targetStage = nullptr)
{
    LiveAppState s;
    s.currentStage     = StringCRC(stage);
    s.isTransitioning  = transitioning;
    s.targetStage      = targetStage ? StringCRC(targetStage) : StringCRC();
    return s;
}

static LiveModuleState MakeModuleState(const char* pu,
                                        const char* mod,
                                        ModuleRuntimeState state)
{
    LiveModuleState s;
    s.puId     = StringCRC(pu);
    s.moduleId = StringCRC(mod);
    s.state    = state;
    return s;
}

static LiveStreamState MakeStreamState(const char* streamId,
                                        unsigned int mps = 0,
                                        unsigned int bps = 0)
{
    LiveStreamState s;
    s.streamId       = StringCRC(streamId);
    s.messagesPerSec = mps;
    s.bytesPerSec    = bps;
    return s;
}

// ==============================================================================
// InspectorLiveStateStore — construction and activity flag
// ==============================================================================

TEST(InspectorLiveStateStore, DefaultConstruct_NotActive)
{
    LiveStateStore store;
    EXPECT_FALSE(store.IsActive());
}

TEST(InspectorLiveStateStore, UpdateAppState_BecomesActive)
{
    LiveStateStore store;
    store.UpdateAppState(MakeAppState("MainMenu"));
    EXPECT_TRUE(store.IsActive());
}

TEST(InspectorLiveStateStore, UpdateAppState_StagePreserved)
{
    LiveStateStore store;
    store.UpdateAppState(MakeAppState("DummyStage"));
    EXPECT_EQ(store.GetAppState().currentStage, StringCRC("DummyStage"));
}

TEST(InspectorLiveStateStore, UpdateAppState_TransitionState)
{
    LiveStateStore store;
    store.UpdateAppState(MakeAppState("StageA", true, "StageB"));

    const LiveAppState& appState = store.GetAppState();
    EXPECT_TRUE(appState.isTransitioning);
    EXPECT_EQ(appState.currentStage, StringCRC("StageA"));
    EXPECT_EQ(appState.targetStage,  StringCRC("StageB"));
}

// ==============================================================================
// InspectorLiveStateStore — module state
// ==============================================================================

TEST(InspectorLiveStateStore, UpdateModuleState_NewEntry_Queryable)
{
    LiveStateStore store;
    store.UpdateModuleState(MakeModuleState("MainPU", "InputModule", ModuleRuntimeState::Running));

    EXPECT_EQ(store.GetModuleState(StringCRC("MainPU"), StringCRC("InputModule")),
              ModuleRuntimeState::Running);
}

TEST(InspectorLiveStateStore, UpdateModuleState_UpdateExisting_Overwrites)
{
    LiveStateStore store;
    store.UpdateModuleState(MakeModuleState("MainPU", "AudioModule", ModuleRuntimeState::Running));
    store.UpdateModuleState(MakeModuleState("MainPU", "AudioModule", ModuleRuntimeState::Failed));

    EXPECT_EQ(store.GetModuleState(StringCRC("MainPU"), StringCRC("AudioModule")),
              ModuleRuntimeState::Failed);
}

TEST(InspectorLiveStateStore, GetModuleState_UnknownModule_ReturnsStopped)
{
    LiveStateStore store;
    EXPECT_EQ(store.GetModuleState(StringCRC("NoSuchPU"), StringCRC("NoSuchModule")),
              ModuleRuntimeState::Stopped);
}

// ==============================================================================
// InspectorLiveStateStore — stream state
// ==============================================================================

TEST(InspectorLiveStateStore, UpdateStreamState_NewEntry_Queryable)
{
    LiveStateStore store;
    store.UpdateStreamState(MakeStreamState("EntityStream", 100, 2048));

    const LiveStreamState* result = store.GetStreamState(StringCRC("EntityStream"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->streamId, StringCRC("EntityStream"));
}

TEST(InspectorLiveStateStore, UpdateStreamState_UpdateExisting_OverwritesThroughput)
{
    LiveStateStore store;
    store.UpdateStreamState(MakeStreamState("PhysicsStream", 50, 1024));
    store.UpdateStreamState(MakeStreamState("PhysicsStream", 200, 4096));

    const LiveStreamState* result = store.GetStreamState(StringCRC("PhysicsStream"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->messagesPerSec, 200u);
    EXPECT_EQ(result->bytesPerSec,   4096u);
}

TEST(InspectorLiveStateStore, GetStreamState_UnknownStream_ReturnsNull)
{
    LiveStateStore store;
    EXPECT_EQ(store.GetStreamState(StringCRC("NoSuchStream")), nullptr);
}

// ==============================================================================
// InspectorLiveStateStore — Clear
// ==============================================================================

TEST(InspectorLiveStateStore, Clear_ResetsAll)
{
    LiveStateStore store;
    store.UpdateAppState(MakeAppState("InGame"));
    store.UpdateModuleState(MakeModuleState("MainPU", "RenderModule", ModuleRuntimeState::Running));

    store.Clear();

    EXPECT_FALSE(store.IsActive());
    EXPECT_EQ(store.GetModuleState(StringCRC("MainPU"), StringCRC("RenderModule")),
              ModuleRuntimeState::Stopped);
}

// ==============================================================================
// InspectorLiveStateStore — multiple independent modules
// ==============================================================================

TEST(InspectorLiveStateStore, MultipleModules_IndependentState)
{
    LiveStateStore store;
    store.UpdateModuleState(MakeModuleState("PU1", "ModA", ModuleRuntimeState::Running));
    store.UpdateModuleState(MakeModuleState("PU1", "ModB", ModuleRuntimeState::Loading));
    store.UpdateModuleState(MakeModuleState("PU2", "ModC", ModuleRuntimeState::Failed));

    EXPECT_EQ(store.GetModuleState(StringCRC("PU1"), StringCRC("ModA")),
              ModuleRuntimeState::Running);
    EXPECT_EQ(store.GetModuleState(StringCRC("PU1"), StringCRC("ModB")),
              ModuleRuntimeState::Loading);
    EXPECT_EQ(store.GetModuleState(StringCRC("PU2"), StringCRC("ModC")),
              ModuleRuntimeState::Failed);
}
