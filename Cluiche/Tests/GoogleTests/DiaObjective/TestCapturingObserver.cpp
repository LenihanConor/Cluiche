#include <gtest/gtest.h>

#include <DiaObjective/Testing/ObjectiveTestHelpers.h>
#include <DiaObjective/IObjectiveObserver.h>
#include <DiaCore/CRC/StringCRC.h>

// ---------------------------------------------------------------------------
// DiaObjective_CapturingObserver — unit tests for the test utility itself
// ---------------------------------------------------------------------------

TEST(DiaObjective_CapturingObserver, InitialState_NoEvents)
{
    Dia::Objective::Testing::CapturingObserver obs;
    EXPECT_EQ(obs.GetEvents().Size(), 0u);
}

TEST(DiaObjective_CapturingObserver, OnObjectiveActivated_RecordsEvent)
{
    Dia::Objective::Testing::CapturingObserver obs;

    obs.OnObjectiveActivated(Dia::Core::StringCRC("ObjA"));

    const auto& events = obs.GetEvents();
    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Activated);
    EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("ObjA").Value());
}

TEST(DiaObjective_CapturingObserver, OnObjectiveCompleted_RecordsEvent)
{
    Dia::Objective::Testing::CapturingObserver obs;

    Dia::Objective::RewardPayload reward;
    obs.OnObjectiveCompleted(Dia::Core::StringCRC("ObjB"), reward);

    const auto& events = obs.GetEvents();
    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Completed);
    EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("ObjB").Value());
}

TEST(DiaObjective_CapturingObserver, OnObjectiveFailed_RecordsEvent)
{
    Dia::Objective::Testing::CapturingObserver obs;

    obs.OnObjectiveFailed(Dia::Core::StringCRC("ObjC"));

    const auto& events = obs.GetEvents();
    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Failed);
    EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("ObjC").Value());
}

TEST(DiaObjective_CapturingObserver, Clear_RemovesAllEvents)
{
    Dia::Objective::Testing::CapturingObserver obs;

    obs.OnObjectiveActivated(Dia::Core::StringCRC("ObjA"));
    obs.OnObjectiveFailed(Dia::Core::StringCRC("ObjB"));
    ASSERT_EQ(obs.GetEvents().Size(), 2u);

    obs.Clear();

    EXPECT_EQ(obs.GetEvents().Size(), 0u);
}

TEST(DiaObjective_CapturingObserver, MultipleEvents_RecordedInOrder)
{
    Dia::Objective::Testing::CapturingObserver obs;

    Dia::Objective::RewardPayload reward;

    obs.OnObjectiveActivated(Dia::Core::StringCRC("Step1"));
    obs.OnObjectiveCompleted(Dia::Core::StringCRC("Step1"), reward);
    obs.OnObjectiveActivated(Dia::Core::StringCRC("Step2"));
    obs.OnObjectiveFailed   (Dia::Core::StringCRC("Step2"));

    const auto& events = obs.GetEvents();
    ASSERT_EQ(events.Size(), 4u);

    EXPECT_EQ(events[0].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Activated);
    EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("Step1").Value());

    EXPECT_EQ(events[1].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Completed);
    EXPECT_EQ(events[1].id.Value(), Dia::Core::StringCRC("Step1").Value());

    EXPECT_EQ(events[2].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Activated);
    EXPECT_EQ(events[2].id.Value(), Dia::Core::StringCRC("Step2").Value());

    EXPECT_EQ(events[3].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Failed);
    EXPECT_EQ(events[3].id.Value(), Dia::Core::StringCRC("Step2").Value());
}
