#include <gtest/gtest.h>
#include <DiaHTN/OperatorRegistry.h>
#include <DiaHTN/TaskResult.h>
#include <DiaCore/CRC/StringCRC.h>

// DiaHTN_OperatorRegistry
// Covers construction, Register, Has, and Find dispatch.

TEST(DiaHTN_OperatorRegistry, DefaultConstruct_Has_ReturnsFalse)
{
    Dia::HTN::OperatorRegistry reg;
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("move")));
}

TEST(DiaHTN_OperatorRegistry, DefaultConstruct_Find_ReturnsNullptr)
{
    Dia::HTN::OperatorRegistry reg;
    EXPECT_EQ(reg.Find(Dia::Core::StringCRC("move")), nullptr);
}

TEST(DiaHTN_OperatorRegistry, Register_Has_ReturnsTrue)
{
    Dia::HTN::OperatorRegistry reg;
    auto fn = [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kSucceeded; };
    reg.Register(Dia::Core::StringCRC("move"), fn);
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("move")));
}

TEST(DiaHTN_OperatorRegistry, Register_Find_ReturnsFn)
{
    Dia::HTN::OperatorRegistry reg;
    auto fn = [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kSucceeded; };
    reg.Register(Dia::Core::StringCRC("move"), fn);
    EXPECT_NE(reg.Find(Dia::Core::StringCRC("move")), nullptr);
}

TEST(DiaHTN_OperatorRegistry, Find_MissingId_ReturnsNullptr)
{
    Dia::HTN::OperatorRegistry reg;
    auto fn = [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kSucceeded; };
    reg.Register(Dia::Core::StringCRC("move"), fn);
    EXPECT_EQ(reg.Find(Dia::Core::StringCRC("attack")), nullptr);
}

TEST(DiaHTN_OperatorRegistry, Register_MultipleOps_AllRetrievable)
{
    Dia::HTN::OperatorRegistry reg;
    auto fnA = [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kSucceeded; };
    auto fnB = [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kRunning; };

    reg.Register(Dia::Core::StringCRC("move"), fnA);
    reg.Register(Dia::Core::StringCRC("attack"), fnB);

    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("move")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("attack")));
}

TEST(DiaHTN_OperatorRegistry, Register_Override_UpdatesFn)
{
    Dia::HTN::OperatorRegistry reg;
    bool secondCalled = false;

    auto fn1 = [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kFailed; };
    auto fn2 = [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult
    {
        *static_cast<bool*>(ctx) = true;
        return Dia::HTN::TaskResult::kSucceeded;
    };

    reg.Register(Dia::Core::StringCRC("move"), fn1);
    reg.Register(Dia::Core::StringCRC("move"), fn2);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    reg.Find(Dia::Core::StringCRC("move"))(&secondCalled, params);
    EXPECT_TRUE(secondCalled);
}

TEST(DiaHTN_OperatorRegistry, OperatorFn_ReturnsRunning)
{
    Dia::HTN::OperatorRegistry reg;
    auto fn = [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kRunning; };
    reg.Register(Dia::Core::StringCRC("patrol"), fn);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    const auto result = reg.Find(Dia::Core::StringCRC("patrol"))(nullptr, params);
    EXPECT_EQ(result, Dia::HTN::TaskResult::kRunning);
}

TEST(DiaHTN_OperatorRegistry, OperatorFn_ReceivesParams)
{
    Dia::HTN::OperatorRegistry reg;
    Dia::Core::StringCRC captured;

    auto fn = [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params)
        -> Dia::HTN::TaskResult
    {
        if (params.Size() > 0)
            *static_cast<Dia::Core::StringCRC*>(ctx) = params[0];
        return Dia::HTN::TaskResult::kSucceeded;
    };

    reg.Register(Dia::Core::StringCRC("move"), fn);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    params.Add(Dia::Core::StringCRC("staging_point"));
    reg.Find(Dia::Core::StringCRC("move"))(&captured, params);
    EXPECT_EQ(captured.Value(), Dia::Core::StringCRC("staging_point").Value());
}
