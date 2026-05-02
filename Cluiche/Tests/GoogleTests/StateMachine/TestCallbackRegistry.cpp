#include <gtest/gtest.h>

#include <DiaStateMachine/CallbackRegistry.h>
#include <DiaStateMachine/StateMachineComponent.h>
#include <DiaStateMachine/FlatStateMachine.h>
#include <DiaStateMachine/StateMachineBuilder.h>

using namespace Dia::StateMachine;

static void SomeAction(void*) {}
static bool SomeGuard(const void*) { return true; }
static void SomeUpdate(void*, float) {}

TEST(CallbackRegistry, RegisterAndFindAction)
{
    CallbackRegistry reg;
    reg.RegisterAction(Dia::Core::StringCRC("run"), SomeAction);

    auto fn = reg.FindAction(Dia::Core::StringCRC("run"));
    EXPECT_EQ(fn, SomeAction);
}

TEST(CallbackRegistry, RegisterAndFindGuard)
{
    CallbackRegistry reg;
    reg.RegisterGuard(Dia::Core::StringCRC("isAlive"), SomeGuard);

    auto fn = reg.FindGuard(Dia::Core::StringCRC("isAlive"));
    EXPECT_EQ(fn, SomeGuard);
}

TEST(CallbackRegistry, RegisterAndFindUpdate)
{
    CallbackRegistry reg;
    reg.RegisterUpdate(Dia::Core::StringCRC("tick"), SomeUpdate);

    auto fn = reg.FindUpdate(Dia::Core::StringCRC("tick"));
    EXPECT_EQ(fn, SomeUpdate);
}

TEST(CallbackRegistry, FindMissingActionReturnsNullptr)
{
    CallbackRegistry reg;
    EXPECT_EQ(reg.FindAction(Dia::Core::StringCRC("missing")), nullptr);
}

TEST(CallbackRegistry, FindMissingGuardReturnsNullptr)
{
    CallbackRegistry reg;
    EXPECT_EQ(reg.FindGuard(Dia::Core::StringCRC("missing")), nullptr);
}

TEST(CallbackRegistry, FindMissingUpdateReturnsNullptr)
{
    CallbackRegistry reg;
    EXPECT_EQ(reg.FindUpdate(Dia::Core::StringCRC("missing")), nullptr);
}

TEST(CallbackRegistry, HasActionBeforeAndAfterRegister)
{
    CallbackRegistry reg;
    EXPECT_FALSE(reg.HasAction(Dia::Core::StringCRC("run")));
    reg.RegisterAction(Dia::Core::StringCRC("run"), SomeAction);
    EXPECT_TRUE(reg.HasAction(Dia::Core::StringCRC("run")));
}

TEST(CallbackRegistry, HasGuardBeforeAndAfterRegister)
{
    CallbackRegistry reg;
    EXPECT_FALSE(reg.HasGuard(Dia::Core::StringCRC("check")));
    reg.RegisterGuard(Dia::Core::StringCRC("check"), SomeGuard);
    EXPECT_TRUE(reg.HasGuard(Dia::Core::StringCRC("check")));
}

TEST(CallbackRegistry, HasUpdateBeforeAndAfterRegister)
{
    CallbackRegistry reg;
    EXPECT_FALSE(reg.HasUpdate(Dia::Core::StringCRC("tick")));
    reg.RegisterUpdate(Dia::Core::StringCRC("tick"), SomeUpdate);
    EXPECT_TRUE(reg.HasUpdate(Dia::Core::StringCRC("tick")));
}

static void ActionOne(void*) {}
static void ActionTwo(void*) {}

TEST(CallbackRegistry, MultipleDistinctRegistrations)
{
    CallbackRegistry reg;

    reg.RegisterAction(Dia::Core::StringCRC("a1"), ActionOne);
    reg.RegisterAction(Dia::Core::StringCRC("a2"), ActionTwo);

    EXPECT_EQ(reg.FindAction(Dia::Core::StringCRC("a1")), ActionOne);
    EXPECT_EQ(reg.FindAction(Dia::Core::StringCRC("a2")), ActionTwo);
}

// ----- StateMachineComponent tests -----

struct CompTestContext {};

TEST(StateMachineComponent, DefaultStateIsNone)
{
    StateMachineComponent comp;
    EXPECT_EQ(comp.GetMachineType(), MachineType::kNone);
    EXPECT_EQ(comp.GetInspectable(), nullptr);
}

TEST(StateMachineComponent, ComponentIdIsCorrect)
{
    EXPECT_EQ(StateMachineComponent::ID, static_cast<Dia::Core::ComponentClassID>(0x534D0001));

    StateMachineComponent comp;
    EXPECT_TRUE(comp.IsType(0x534D0001));
    EXPECT_FALSE(comp.IsType(0x00000001));
    EXPECT_EQ(comp.GetType(), static_cast<Dia::Core::ComponentClassID>(0x534D0001));
}

TEST(StateMachineComponent, AttachFlatMachineAndGet)
{
    CompTestContext ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("Idle"))
        .InitialState(Dia::Core::StringCRC("Idle"))
        .Build();

    FlatStateMachine<CompTestContext> fsm(
        Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def),
        ctx);

    StateMachineComponent comp;
    comp.AttachMachine(&fsm, MachineType::kFlat);

    EXPECT_EQ(comp.GetMachineType(), MachineType::kFlat);
    EXPECT_EQ(comp.GetInspectable(), &fsm);
}

TEST(StateMachineComponent, GetMachineTypedReturnsCast)
{
    CompTestContext ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("Idle"))
        .InitialState(Dia::Core::StringCRC("Idle"))
        .Build();

    FlatStateMachine<CompTestContext> fsm(
        Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def),
        ctx);

    StateMachineComponent comp;
    comp.AttachMachine(&fsm, MachineType::kFlat);

    auto* typed = comp.GetMachine<FlatStateMachine<CompTestContext>>();
    EXPECT_EQ(typed, &fsm);
    EXPECT_EQ(typed->GetCurrentStateId(), Dia::Core::StringCRC("Idle"));
}

TEST(StateMachineComponent, AttachNullClearsState)
{
    CompTestContext ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("Idle"))
        .InitialState(Dia::Core::StringCRC("Idle"))
        .Build();

    FlatStateMachine<CompTestContext> fsm(
        Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def),
        ctx);

    StateMachineComponent comp;
    comp.AttachMachine(&fsm, MachineType::kFlat);
    comp.AttachMachine(nullptr, MachineType::kNone);

    EXPECT_EQ(comp.GetMachineType(), MachineType::kNone);
    EXPECT_EQ(comp.GetInspectable(), nullptr);
}

// ---------------------------------------------------------------------------
// CallbackRegistry::Finalize() tests
// ---------------------------------------------------------------------------

TEST(CallbackRegistry, FinalizeSetsFinalizedFlag)
{
    CallbackRegistry reg;
    EXPECT_FALSE(reg.IsFinalized());
    reg.Finalize();
    EXPECT_TRUE(reg.IsFinalized());
}

TEST(CallbackRegistry, FinalizeAllowsQueryAfterwards)
{
    CallbackRegistry reg;
    reg.RegisterAction(Dia::Core::StringCRC("run"), SomeAction);
    reg.Finalize();

    EXPECT_TRUE(reg.IsFinalized());
    EXPECT_EQ(reg.FindAction(Dia::Core::StringCRC("run")), SomeAction);
    EXPECT_TRUE(reg.HasAction(Dia::Core::StringCRC("run")));
}

TEST(CallbackRegistry, DoubleFinalizeAsserts)
{
    CallbackRegistry reg;
    reg.Finalize();
    EXPECT_DEATH(reg.Finalize(), "");
}

TEST(CallbackRegistry, RegisterActionAfterFinalizeAsserts)
{
    CallbackRegistry reg;
    reg.Finalize();
    EXPECT_DEATH(reg.RegisterAction(Dia::Core::StringCRC("run"), SomeAction), "");
}

TEST(CallbackRegistry, RegisterGuardAfterFinalizeAsserts)
{
    CallbackRegistry reg;
    reg.Finalize();
    EXPECT_DEATH(reg.RegisterGuard(Dia::Core::StringCRC("check"), SomeGuard), "");
}

TEST(CallbackRegistry, RegisterUpdateAfterFinalizeAsserts)
{
    CallbackRegistry reg;
    reg.Finalize();
    EXPECT_DEATH(reg.RegisterUpdate(Dia::Core::StringCRC("tick"), SomeUpdate), "");
}
