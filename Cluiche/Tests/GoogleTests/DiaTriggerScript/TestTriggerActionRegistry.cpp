#include <gtest/gtest.h>

#include <DiaTriggerScript/TriggerActionRegistry.h>
#include <DiaTriggerScript/ITriggerActionHandler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    // Concrete handler that records how many times Execute() was called.
    class CountingHandler : public Dia::TriggerScript::ITriggerActionHandler
    {
    public:
        int callCount = 0;
        void Execute(const Dia::TriggerScript::ActionContext&) override { ++callCount; }
    };

    Json::Value EmptyParams() { return Json::Value(Json::objectValue); }
}

// ---------------------------------------------------------------------------
// DiaTriggerScript_ActionRegistry — empty registry
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_ActionRegistry, DefaultConstruct_Has_ReturnsFalse)
{
    Dia::TriggerScript::TriggerActionRegistry registry;
    EXPECT_FALSE(registry.Has(Dia::Core::StringCRC("fire")));
}

// ---------------------------------------------------------------------------
// Register + Has
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_ActionRegistry, Register_Has_ReturnsTrue)
{
    Dia::TriggerScript::TriggerActionRegistry registry;
    CountingHandler handler;

    registry.Register(Dia::Core::StringCRC("spawn"), &handler);

    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("spawn")));
}

TEST(DiaTriggerScript_ActionRegistry, Register_DifferentKey_Has_ReturnsFalse)
{
    Dia::TriggerScript::TriggerActionRegistry registry;
    CountingHandler handler;

    registry.Register(Dia::Core::StringCRC("spawn"), &handler);

    EXPECT_FALSE(registry.Has(Dia::Core::StringCRC("fire")));
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_ActionRegistry, Dispatch_RegisteredHandler_ExecuteCalled)
{
    Dia::TriggerScript::TriggerActionRegistry registry;
    CountingHandler handler;

    registry.Register(Dia::Core::StringCRC("fire_event"), &handler);

    Json::Value params = EmptyParams();
    Dia::TriggerScript::ActionContext ctx{ Dia::Core::StringCRC("trig-1"), params };
    registry.Dispatch(Dia::Core::StringCRC("fire_event"), ctx);

    EXPECT_EQ(handler.callCount, 1);
}

TEST(DiaTriggerScript_ActionRegistry, Dispatch_Twice_ExecuteCalledTwice)
{
    Dia::TriggerScript::TriggerActionRegistry registry;
    CountingHandler handler;

    registry.Register(Dia::Core::StringCRC("give_gold"), &handler);

    Json::Value params = EmptyParams();
    Dia::TriggerScript::ActionContext ctx{ Dia::Core::StringCRC("trig-2"), params };
    registry.Dispatch(Dia::Core::StringCRC("give_gold"), ctx);
    registry.Dispatch(Dia::Core::StringCRC("give_gold"), ctx);

    EXPECT_EQ(handler.callCount, 2);
}

TEST(DiaTriggerScript_ActionRegistry, Dispatch_TriggerId_PassedToHandler)
{
    struct CapturingHandler : public Dia::TriggerScript::ITriggerActionHandler
    {
        Dia::Core::StringCRC lastTriggerId;
        void Execute(const Dia::TriggerScript::ActionContext& ctx) override
        {
            lastTriggerId = ctx.triggerId;
        }
    };

    Dia::TriggerScript::TriggerActionRegistry registry;
    CapturingHandler handler;

    registry.Register(Dia::Core::StringCRC("act"), &handler);

    Json::Value params = EmptyParams();
    Dia::TriggerScript::ActionContext ctx{ Dia::Core::StringCRC("castle-door"), params };
    registry.Dispatch(Dia::Core::StringCRC("act"), ctx);

    EXPECT_EQ(handler.lastTriggerId, Dia::Core::StringCRC("castle-door"));
}

// ---------------------------------------------------------------------------
// Overwrite: last write wins
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_ActionRegistry, Register_Duplicate_LastHandlerDispatched)
{
    Dia::TriggerScript::TriggerActionRegistry registry;
    CountingHandler first;
    CountingHandler second;

    registry.Register(Dia::Core::StringCRC("act"), &first);
    registry.Register(Dia::Core::StringCRC("act"), &second);

    Json::Value params = EmptyParams();
    Dia::TriggerScript::ActionContext ctx{ Dia::Core::StringCRC("t"), params };
    registry.Dispatch(Dia::Core::StringCRC("act"), ctx);

    EXPECT_EQ(first.callCount,  0);
    EXPECT_EQ(second.callCount, 1);
}

// ---------------------------------------------------------------------------
// Two independent registries: no cross-contamination
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_ActionRegistry, TwoRegistries_SameKey_DoNotInterfere)
{
    Dia::TriggerScript::TriggerActionRegistry regA;
    Dia::TriggerScript::TriggerActionRegistry regB;
    CountingHandler handlerA;
    CountingHandler handlerB;

    regA.Register(Dia::Core::StringCRC("act"), &handlerA);
    regB.Register(Dia::Core::StringCRC("act"), &handlerB);

    Json::Value params = EmptyParams();
    Dia::TriggerScript::ActionContext ctx{ Dia::Core::StringCRC("t"), params };
    regA.Dispatch(Dia::Core::StringCRC("act"), ctx);

    EXPECT_EQ(handlerA.callCount, 1);
    EXPECT_EQ(handlerB.callCount, 0);
}

TEST(DiaTriggerScript_ActionRegistry, TwoRegistries_RegisterInOne_OtherHasNothing)
{
    Dia::TriggerScript::TriggerActionRegistry regA;
    Dia::TriggerScript::TriggerActionRegistry regB;
    CountingHandler handler;

    regA.Register(Dia::Core::StringCRC("act"), &handler);

    EXPECT_TRUE (regA.Has(Dia::Core::StringCRC("act")));
    EXPECT_FALSE(regB.Has(Dia::Core::StringCRC("act")));
}
