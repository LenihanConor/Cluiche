#include <gtest/gtest.h>
#include <Modules/TestStages/TestResultsRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace CluicheTest;

// T7-T9 from visual-feedback.md require a live ImGui frame + full PU stack —
// those are covered by E2E scenarios, not unit tests.
//
// This file covers the checkpoint-aware SetRunning overload (added alongside
// the HUD implementation) which has no E2E equivalent.

class TestStageHUDRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override   { TestResultsRegistry::Create(); }
    void TearDown() override { TestResultsRegistry::Destroy(); }
    TestResultsRegistry& Registry() { return TestResultsRegistry::GetInstance(); }
};

TEST_F(TestStageHUDRegistryTest, SetRunningWithCheckpointsStoresNames)
{
    Dia::Core::StringCRC cp1("cp.one");
    Dia::Core::StringCRC cp2("cp.two");
    const Dia::Core::StringCRC cps[] = { cp1, cp2 };

    Registry().SetRunning(Dia::Core::StringCRC("Stage"), 100, cps, 2);

    const StageResult* r = Registry().GetResult(Dia::Core::StringCRC("Stage"));
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->checkpoints.Size(), 2u);
    EXPECT_EQ(r->checkpoints[0], cp1);
    EXPECT_EQ(r->checkpoints[1], cp2);
}

TEST_F(TestStageHUDRegistryTest, SetRunningClearsCheckpointsOnRestart)
{
    Dia::Core::StringCRC cp1("old.checkpoint");
    const Dia::Core::StringCRC oldCps[] = { cp1 };
    Registry().SetRunning(Dia::Core::StringCRC("Stage"), 100, oldCps, 1);

    Registry().SetRunning(Dia::Core::StringCRC("Stage"), 200);

    const StageResult* r = Registry().GetResult(Dia::Core::StringCRC("Stage"));
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->checkpoints.Size(), 0u);
    EXPECT_EQ(r->budgetFrames, 200u);
}

TEST_F(TestStageHUDRegistryTest, SetRunningWithZeroCheckpointsIsValid)
{
    Registry().SetRunning(Dia::Core::StringCRC("Stage"), 50, nullptr, 0);

    const StageResult* r = Registry().GetResult(Dia::Core::StringCRC("Stage"));
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->checkpoints.Size(), 0u);
}

TEST_F(TestStageHUDRegistryTest, SetRunningClampsToMaxCheckpoints)
{
    Dia::Core::StringCRC cps[16];
    for (unsigned int i = 0; i < 16; ++i)
        cps[i] = Dia::Core::StringCRC("cp");

    Registry().SetRunning(Dia::Core::StringCRC("Stage"), 100, cps, 16);

    const StageResult* r = Registry().GetResult(Dia::Core::StringCRC("Stage"));
    ASSERT_NE(r, nullptr);
    EXPECT_LE(r->checkpoints.Size(), StageResult::kMaxCheckpoints);
}
