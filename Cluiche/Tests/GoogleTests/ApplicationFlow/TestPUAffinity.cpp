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
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
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

// ---------------------------------------------------------------------------
// PUPlacementAssert — proves ProcessingUnit::AddModule's kAllowedPUs guard:
//   1. fires in Release builds too (RELEASE_DIA_ASSERT), not just Debug
//   2. closes the kAny-PU gap: a kAny-affinity PU (e.g. a custom-named PU
//      like an editor's tool PU) can no longer silently accept a module
//      that declares a specific kAllowedPUs role
//   3. does NOT fire for correctly-placed / unannotated modules
//
// ProcessingUnit::AddModule reads Module::GetTypeId(), which is only set by
// Application::BuildFromManifest() (Module::SetTypeId is private, friended
// to ProcessingUnit/Application only — a bare Module constructed directly in
// a test has an empty typeId). So these tests go through the real
// Application::Start() -> BuildFromManifest() -> ProcessingUnit::AddModule()
// path, using TypeRegistry::Global() (the same registry AddModule reads
// from) rather than a local TypeRegistry instance.
// ---------------------------------------------------------------------------
namespace {

    // Register a module type into the GLOBAL TypeRegistry (mirrors
    // RegisterInto<T> above, but targets TypeRegistry::Global() since that's
    // what ProcessingUnit::AddModule consults). TypeRegistry::Register()
    // silently skips duplicate registrations, so calling this at the top of
    // every test in this group is safe.
    template<typename T>
    void RegisterGlobal()
    {
        TypeRegistry::TypeMetadata meta;
        meta.factory = [](const StringCRC& id) -> Module* { return new T(id); };
        if constexpr (HasAllowedPUs<T>::value)
            meta.allowedPUs = T::kAllowedPUs;
        if constexpr (HasDescription<T>::value)
            meta.description = T::kDescription;
        TypeRegistry::Global().Register(T::kTypeId, meta);
    }

    void EnsureGlobalPlacementRegistrations()
    {
        RegisterGlobal<SimOnlyModule>();
        RegisterGlobal<NoAnnotationModule>();
    }

    // Minimal single-stage, single-PU, single-module manifest — just enough
    // to pass ManifestValidatorV2 and reach BuildFromManifest().
    ApplicationManifestV3 BuildPlacementManifest(const char* puInstanceId,
                                                  const StringCRC& moduleTypeId)
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;

        StageDeclaration boot;
        boot.name = StringCRC("Boot");
        manifest.stages.Add(boot);
        manifest.initialStage = StringCRC("Boot");

        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC(puInstanceId);
        pu.frequencyHz     = 30.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("placementMod");
        mod.typeId         = moduleTypeId;
        mod.startTimeoutMs = 1000.0f;
        mod.stopTimeoutMs  = 1000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);

        manifest.processingUnits.Add(pu);
        return manifest;
    }

} // anonymous namespace

// Case 1 (finding 1): moduleAffinity=kSim vs PU affinity=kMain ("MainPU").
// Mismatch. Must fire even without a debugger attached, in any build config
// — this is exactly what RELEASE_DIA_ASSERT (routed through BREAKPOINT() /
// __debugbreak()) is for.
TEST(PUPlacementAssert, Mismatch_MainPU_Dies)
{
    EnsureGlobalPlacementRegistrations();
    ApplicationManifestV3 manifest = BuildPlacementManifest("MainPU", SimOnlyModule::kTypeId);

    EXPECT_DEATH(
        {
            Application app(manifest, TypeRegistry::Global());
            app.Start();
        },
        "");
}

// Case 2 (finding 2 — the kAny-PU gap regression test): moduleAffinity=kSim
// vs a custom-named PU ("CustomToolPU"), which ProcessingUnit's constructor
// maps to PUAffinity::kAny. Before this task's fix, PUAffinity::kAny is 0xFF
// so HasAffinity(kSim, kAny) was unconditionally true and this mismatch
// silently passed forever, in every build config. Must now die too.
TEST(PUPlacementAssert, Mismatch_CustomAnyPU_Dies)
{
    EnsureGlobalPlacementRegistrations();
    ApplicationManifestV3 manifest = BuildPlacementManifest("CustomToolPU", SimOnlyModule::kTypeId);

    EXPECT_DEATH(
        {
            Application app(manifest, TypeRegistry::Global());
            app.Start();
        },
        "");
}

// Case 3a: an unannotated module (defaults to kAllowedPUs = kAny) is valid
// on any PU, including a custom-named (kAny-affinity) one. No crash.
TEST(PUPlacementAssert, NoAnnotationModule_AnyPU_Succeeds)
{
    EnsureGlobalPlacementRegistrations();
    ApplicationManifestV3 manifest = BuildPlacementManifest("CustomToolPU", NoAnnotationModule::kTypeId);

    Application app(manifest, TypeRegistry::Global());
    EXPECT_TRUE(app.Start());

    Dia::Core::Containers::DynamicArrayC<ModuleStateInfo, 64> infos;
    app.GetActiveModules(StringCRC("CustomToolPU"), infos);
    ASSERT_EQ(infos.Size(), 1u);
    EXPECT_TRUE(infos[0].instanceId == StringCRC("placementMod"));
}

// Case 3b: a kSim-only module placed on "SimPU" (correct placement). No crash.
TEST(PUPlacementAssert, SimOnlyModule_SimPU_Succeeds)
{
    EnsureGlobalPlacementRegistrations();
    ApplicationManifestV3 manifest = BuildPlacementManifest("SimPU", SimOnlyModule::kTypeId);

    Application app(manifest, TypeRegistry::Global());
    EXPECT_TRUE(app.Start());

    Dia::Core::Containers::DynamicArrayC<ModuleStateInfo, 64> infos;
    app.GetActiveModules(StringCRC("SimPU"), infos);
    ASSERT_EQ(infos.Size(), 1u);
    EXPECT_TRUE(infos[0].instanceId == StringCRC("placementMod"));
}
