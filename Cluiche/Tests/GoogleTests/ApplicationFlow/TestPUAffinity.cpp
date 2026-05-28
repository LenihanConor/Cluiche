////////////////////////////////////////////////////////////////////////////////
// Filename: TestPUAffinity.cpp
// GoogleTest suite — DiaApplicationFlow PUAffinity, TypeRegistry metadata,
//                    and ProcessingUnit affinity mapping.
//
// Covers:
//   - HasAffinity() bitmask logic
//   - TypeRegistry metadata storage (allowedPUs, description)
//   - ModuleRegistration<T> SFINAE capture of kAllowedPUs / kDescription
//   - ProcessingUnit::GetAffinity() identity mapping
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Minimal module helpers — defined in anonymous namespace to avoid ODR
// collisions with other test translation units.
// ---------------------------------------------------------------------------
namespace {

    // A bare module with no metadata annotations (tests default-to-kAny path).
    struct NoAnnotationModule : Module
    {
        using Module::Module;
        static const StringCRC kTypeId;
        StartResult DoStart() override { return StartResult::kReady; }
        void        DoUpdate(float) override {}
        StopResult  DoStop()  override { return StopResult::kDone; }
    };
    const StringCRC NoAnnotationModule::kTypeId("PATest_NoAnnotationModule");

    // A module annotated with kAllowedPUs = kSim.
    struct SimOnlyModule : Module
    {
        using Module::Module;
        static const StringCRC kTypeId;
        static constexpr PUAffinity kAllowedPUs = PUAffinity::kSim;
        StartResult DoStart() override { return StartResult::kReady; }
        void        DoUpdate(float) override {}
        StopResult  DoStop()  override { return StopResult::kDone; }
    };
    const StringCRC SimOnlyModule::kTypeId("PATest_SimOnlyModule");

    // A module annotated with both kAllowedPUs and kDescription.
    struct DescribedModule : Module
    {
        using Module::Module;
        static const StringCRC kTypeId;
        static constexpr PUAffinity kAllowedPUs  = PUAffinity::kMain;
        static constexpr const char* kDescription = "test desc";
        StartResult DoStart() override { return StartResult::kReady; }
        void        DoUpdate(float) override {}
        StopResult  DoStop()  override { return StopResult::kDone; }
    };
    const StringCRC DescribedModule::kTypeId("PATest_DescribedModule");

    // A module annotated with kAllowedPUs but no kDescription.
    struct NoDescModule : Module
    {
        using Module::Module;
        static const StringCRC kTypeId;
        static constexpr PUAffinity kAllowedPUs = PUAffinity::kRender;
        StartResult DoStart() override { return StartResult::kReady; }
        void        DoUpdate(float) override {}
        StopResult  DoStop()  override { return StopResult::kDone; }
    };
    const StringCRC NoDescModule::kTypeId("PATest_NoDescModule");

    // Helper: register a module type T into a local TypeRegistry using
    // ModuleRegistration-equivalent logic (inline, avoids global-registry pollution).
    template<typename T>
    void RegisterInto(TypeRegistry& reg)
    {
        TypeRegistry::TypeMetadata meta;
        meta.factory = [](const StringCRC& id) -> Module* { return new T(id); };
        if constexpr (HasAllowedPUs<T>::value)
            meta.allowedPUs = T::kAllowedPUs;
        if constexpr (HasDescription<T>::value)
            meta.description = T::kDescription;
        reg.Register(T::kTypeId, meta);
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// HasAffinity bitmask tests
// ---------------------------------------------------------------------------

TEST(HasAffinityTest, kMain_MatchesMain)
{
    EXPECT_TRUE(HasAffinity(PUAffinity::kMain, PUAffinity::kMain));
}

TEST(HasAffinityTest, kMain_DoesNotMatchSim)
{
    EXPECT_FALSE(HasAffinity(PUAffinity::kMain, PUAffinity::kSim));
}

TEST(HasAffinityTest, kAny_MatchesMain)
{
    EXPECT_TRUE(HasAffinity(PUAffinity::kAny, PUAffinity::kMain));
}

TEST(HasAffinityTest, kAny_MatchesSim)
{
    EXPECT_TRUE(HasAffinity(PUAffinity::kAny, PUAffinity::kSim));
}

TEST(HasAffinityTest, kAny_MatchesRender)
{
    EXPECT_TRUE(HasAffinity(PUAffinity::kAny, PUAffinity::kRender));
}

TEST(HasAffinityTest, BitwiseOr_MainOrSim_MatchesBoth)
{
    constexpr PUAffinity mainOrSim = PUAffinity::kMain | PUAffinity::kSim;
    EXPECT_TRUE(HasAffinity(mainOrSim, PUAffinity::kMain));
    EXPECT_TRUE(HasAffinity(mainOrSim, PUAffinity::kSim));
    EXPECT_FALSE(HasAffinity(mainOrSim, PUAffinity::kRender));
}

// ---------------------------------------------------------------------------
// TypeRegistry metadata tests
// ---------------------------------------------------------------------------

TEST(TypeRegistryMetadata, NoAnnotation_DefaultsToKAny)
{
    TypeRegistry reg;
    RegisterInto<NoAnnotationModule>(reg);
    EXPECT_EQ(reg.GetAllowedPUs(NoAnnotationModule::kTypeId), PUAffinity::kAny);
}

TEST(TypeRegistryMetadata, WithAnnotation_ReturnsCorrectAffinity)
{
    TypeRegistry reg;
    RegisterInto<SimOnlyModule>(reg);
    EXPECT_EQ(reg.GetAllowedPUs(SimOnlyModule::kTypeId), PUAffinity::kSim);
}

TEST(TypeRegistryMetadata, WithDescription_ReturnsCorrectDescription)
{
    TypeRegistry reg;
    RegisterInto<DescribedModule>(reg);
    const char* desc = reg.GetDescription(DescribedModule::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_STREQ(desc, "test desc");
}

TEST(TypeRegistryMetadata, NoDescription_ReturnsNullptr)
{
    TypeRegistry reg;
    RegisterInto<NoDescModule>(reg);
    EXPECT_EQ(reg.GetDescription(NoDescModule::kTypeId), nullptr);
}

// ---------------------------------------------------------------------------
// ProcessingUnit affinity mapping tests
// ---------------------------------------------------------------------------

TEST(ProcessingUnitAffinity, MainPU_HasMainAffinity)
{
    ProcessingUnit pu(StringCRC("MainPU"), 30.0f, false);
    EXPECT_EQ(pu.GetAffinity(), PUAffinity::kMain);
}

TEST(ProcessingUnitAffinity, SimPU_HasSimAffinity)
{
    ProcessingUnit pu(StringCRC("SimPU"), 30.0f, true);
    EXPECT_EQ(pu.GetAffinity(), PUAffinity::kSim);
}

TEST(ProcessingUnitAffinity, RenderPU_HasRenderAffinity)
{
    ProcessingUnit pu(StringCRC("RenderPU"), 60.0f, true);
    EXPECT_EQ(pu.GetAffinity(), PUAffinity::kRender);
}

TEST(ProcessingUnitAffinity, UnknownPU_HasAnyAffinity)
{
    ProcessingUnit pu(StringCRC("CustomPU"), 30.0f, false);
    EXPECT_EQ(pu.GetAffinity(), PUAffinity::kAny);
}
