////////////////////////////////////////////////////////////////////////////////
// Filename: TestValidation.cpp
// GoogleTest suite — DiaApplicationFlow v2 ManifestValidatorV2
//
// Covers every error and warning code produced by ManifestValidatorV2.
// Each test uses a local TypeRegistry to avoid global state pollution.
// Module type prefixed "Val_" to avoid ODR collisions with other test files.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaApplicationFlow/Manifest/ManifestValidatorV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Fixture module — minimal concrete Module for the registry
// ---------------------------------------------------------------------------

struct Val_SimpleModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;
    StartResult DoStart() override { return StartResult::kReady; }
    void        DoUpdate(float) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC Val_SimpleModule::kTypeId("Val_SimpleModule");

// ---------------------------------------------------------------------------
// Helper: build a fully valid single-PU, single-module manifest.
// All tests that need a clean baseline start from this.
// ---------------------------------------------------------------------------

static TypeRegistry BuildRegistryWithSimple()
{
    TypeRegistry reg;
    reg.Register(Val_SimpleModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new Val_SimpleModule(id); });
    return reg;
}

static ApplicationManifestV2 BuildValidManifest()
{
    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("mod0");
    mod.typeId         = Val_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));

    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);
    return manifest;
}

// Search results array for an entry matching a specific code string.
static bool HasCode(
    const DynamicArrayC<ValidationEntry, 64>& results,
    const char* code,
    ValidationSeverity severity = ValidationSeverity::kError)
{
    const StringCRC target(code);
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        if (results[i].code == target && results[i].severity == severity)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// A properly formed manifest produces no errors and no warnings.
TEST(Validation, ValidManifestHasNoErrors)
{
    TypeRegistry reg = BuildRegistryWithSimple();
    ApplicationManifestV2 manifest = BuildValidManifest();

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(validator.HasErrors())
        << "Valid manifest should produce no errors";
    EXPECT_FALSE(validator.HasWarnings())
        << "Valid manifest should produce no warnings";
    EXPECT_EQ(validator.GetResults().Size(), 0u);
}

// A module whose typeId is not in the TypeRegistry produces UNKNOWN_TYPE.
TEST(Validation, UnknownTypeIdReportsError)
{
    TypeRegistry reg;
    // Register nothing — typeId "Val_Ghost" is unknown.

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("ghostMod");
    mod.typeId         = StringCRC("Val_Ghost");
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "UNKNOWN_TYPE"))
        << "Expected UNKNOWN_TYPE error for unregistered typeId";
}

// initialStage references a stage name not present in the stages array.
TEST(Validation, UnknownInitialStageReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Nonexistent");  // not in stages

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("mod0");
    mod.typeId         = Val_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "UNKNOWN_STAGE"))
        << "Expected UNKNOWN_STAGE error for missing initialStage";
}

// Two ProcessingUnits share the same instanceId — produces DUPLICATE_PU_ID.
TEST(Validation, DuplicatePUIdReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    // First PU
    {
        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC("MainPU");
        pu.frequencyHz     = 60.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("mod0");
        mod.typeId         = Val_SimpleModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
        manifest.processingUnits.Add(pu);
    }

    // Second PU — same instanceId as the first
    {
        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC("MainPU");  // duplicate
        pu.frequencyHz     = 30.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("mod1");
        mod.typeId         = Val_SimpleModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
        manifest.processingUnits.Add(pu);
    }

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "DUPLICATE_PU_ID"))
        << "Expected DUPLICATE_PU_ID error";
}

// Two streams share the same id — produces DUPLICATE_STREAM_ID.
TEST(Validation, DuplicateStreamIdReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();
    ApplicationManifestV2 manifest = BuildValidManifest();

    // Add a second PU so stream PU refs are valid.
    ProcessingUnitDeclaration pu2;
    pu2.instanceId      = StringCRC("SecondPU");
    pu2.frequencyHz     = 30.0f;
    pu2.dedicatedThread = false;

    ModuleDeclaration mod2;
    mod2.instanceId     = StringCRC("mod2");
    mod2.typeId         = Val_SimpleModule::kTypeId;
    mod2.startTimeoutMs = 10000.0f;
    mod2.stopTimeoutMs  = 5000.0f;
    mod2.stages.Add(StringCRC("Boot"));
    pu2.modules.Add(mod2);
    manifest.processingUnits.Add(pu2);

    // Two streams with the same id.
    StreamDeclaration s1;
    s1.id          = StringCRC("DataStream");
    s1.kind        = StringCRC("EventStream");
    s1.fromPU      = StringCRC("MainPU");
    s1.toPU        = StringCRC("SecondPU");
    s1.multiWriter = false;

    StreamDeclaration s2;
    s2.id          = StringCRC("DataStream");  // duplicate
    s2.kind        = StringCRC("EventStream");
    s2.fromPU      = StringCRC("MainPU");
    s2.toPU        = StringCRC("SecondPU");
    s2.multiWriter = false;

    manifest.streams.Add(s1);
    manifest.streams.Add(s2);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "DUPLICATE_STREAM_ID"))
        << "Expected DUPLICATE_STREAM_ID error";
}

// A stream's fromPU references a PU that doesn't exist — produces UNKNOWN_PU.
TEST(Validation, UnknownStreamPUReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();
    ApplicationManifestV2 manifest = BuildValidManifest();

    StreamDeclaration s;
    s.id          = StringCRC("MyStream");
    s.kind        = StringCRC("EventStream");
    s.fromPU      = StringCRC("GhostPU");  // does not exist in manifest
    s.toPU        = StringCRC("MainPU");
    s.multiWriter = false;
    manifest.streams.Add(s);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "UNKNOWN_PU"))
        << "Expected UNKNOWN_PU error for stream fromPU referencing missing PU";
}

// Two modules in the same PU share the same instanceId — DUPLICATE_MODULE_ID.
TEST(Validation, DuplicateModuleIdReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // Two modules, same instanceId.
    for (int i = 0; i < 2; ++i)
    {
        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("sameMod");  // duplicate
        mod.typeId         = Val_SimpleModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
    }
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "DUPLICATE_MODULE_ID"))
        << "Expected DUPLICATE_MODULE_ID error";
}

// A module declares a dependency on an instanceId that doesn't exist in the
// same PU — produces UNKNOWN_DEPENDENCY.
TEST(Validation, UnknownDependencyReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("mod0");
    mod.typeId         = Val_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    mod.dependencies.Add(StringCRC("ghostDep"));  // not in this PU
    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "UNKNOWN_DEPENDENCY"))
        << "Expected UNKNOWN_DEPENDENCY error";
}

// A module depends on another module that appears LATER in the array.  The
// framework uses array order as startup order, so this would start the
// dependent before its dependency — must produce DEPENDENCY_ORDER.
TEST(Validation, DependencyOrderReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // modA declared FIRST but depends on modB which comes later.
    ModuleDeclaration modA;
    modA.instanceId     = StringCRC("modA");
    modA.typeId         = Val_SimpleModule::kTypeId;
    modA.startTimeoutMs = 10000.0f;
    modA.stopTimeoutMs  = 5000.0f;
    modA.stages.Add(StringCRC("Boot"));
    modA.dependencies.Add(StringCRC("modB"));
    pu.modules.Add(modA);

    // modB — at a LATER index than its dependent modA.
    ModuleDeclaration modB;
    modB.instanceId     = StringCRC("modB");
    modB.typeId         = Val_SimpleModule::kTypeId;
    modB.startTimeoutMs = 10000.0f;
    modB.stopTimeoutMs  = 5000.0f;
    modB.stages.Add(StringCRC("Boot"));
    pu.modules.Add(modB);

    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "DEPENDENCY_ORDER"))
        << "Expected DEPENDENCY_ORDER error when a module depends on a later-indexed module";
}

// A module depending on a module at a lower (earlier) index is valid — no
// DEPENDENCY_ORDER error is produced.
TEST(Validation, DependencyOrderValidWhenDepIsEarlier)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // modB first — it's the dependency.
    ModuleDeclaration modB;
    modB.instanceId     = StringCRC("modB");
    modB.typeId         = Val_SimpleModule::kTypeId;
    modB.startTimeoutMs = 10000.0f;
    modB.stopTimeoutMs  = 5000.0f;
    modB.stages.Add(StringCRC("Boot"));
    pu.modules.Add(modB);

    // modA second — correctly depends on modB (at a lower index).
    ModuleDeclaration modA;
    modA.instanceId     = StringCRC("modA");
    modA.typeId         = Val_SimpleModule::kTypeId;
    modA.startTimeoutMs = 10000.0f;
    modA.stopTimeoutMs  = 5000.0f;
    modA.stages.Add(StringCRC("Boot"));
    modA.dependencies.Add(StringCRC("modB"));
    pu.modules.Add(modA);

    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(HasCode(validator.GetResults(), "DEPENDENCY_ORDER"))
        << "Expected no DEPENDENCY_ORDER error when dep is at an earlier index";
}

// Two modules depend on each other (A → B, B → A) — produces CYCLE_DETECTED.
TEST(Validation, CycleDetectedReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // modA depends on modB
    ModuleDeclaration modA;
    modA.instanceId     = StringCRC("modA");
    modA.typeId         = Val_SimpleModule::kTypeId;
    modA.startTimeoutMs = 10000.0f;
    modA.stopTimeoutMs  = 5000.0f;
    modA.stages.Add(StringCRC("Boot"));
    modA.dependencies.Add(StringCRC("modB"));
    pu.modules.Add(modA);

    // modB depends on modA — creates the cycle
    ModuleDeclaration modB;
    modB.instanceId     = StringCRC("modB");
    modB.typeId         = Val_SimpleModule::kTypeId;
    modB.startTimeoutMs = 10000.0f;
    modB.stopTimeoutMs  = 5000.0f;
    modB.stages.Add(StringCRC("Boot"));
    modB.dependencies.Add(StringCRC("modA"));
    pu.modules.Add(modB);

    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "CYCLE_DETECTED"))
        << "Expected CYCLE_DETECTED error for mutual dependency";
}

// A stream has multiWriter=false but two modules list it in their writes array
// — produces MULTI_WRITER_VIOLATION.
TEST(Validation, MultiWriterViolationReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    StreamDeclaration stream;
    stream.id          = StringCRC("SharedStream");
    stream.kind        = StringCRC("EventStream");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    stream.multiWriter = false;  // single writer only
    manifest.streams.Add(stream);

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // Both modules write the same stream.
    for (int i = 0; i < 2; ++i)
    {
        ModuleDeclaration mod;
        char buf[16];
        buf[0] = 'w'; buf[1] = char('0' + i); buf[2] = '\0';
        mod.instanceId     = StringCRC(buf);
        mod.typeId         = Val_SimpleModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        mod.writes.Add(StringCRC("SharedStream"));
        pu.modules.Add(mod);
    }
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "MULTI_WRITER_VIOLATION"))
        << "Expected MULTI_WRITER_VIOLATION error";
}

// A module's stages array references a stage not declared in manifest.stages
// — produces ORPHAN_MODULE.
TEST(Validation, OrphanModuleReportsError)
{
    TypeRegistry reg = BuildRegistryWithSimple();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // Module references a stage that is not declared.
    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("orphanMod");
    mod.typeId         = Val_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("UndeclaredStage"));
    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "ORPHAN_MODULE"))
        << "Expected ORPHAN_MODULE error for module with no matching declared stage";
}

// A stream is declared but no module lists it in its writes array.
// This is a warning (ORPHAN_STREAM), not an error.
TEST(Validation, OrphanStreamReportsWarning)
{
    TypeRegistry reg = BuildRegistryWithSimple();
    ApplicationManifestV2 manifest = BuildValidManifest();

    // Stream declared but never written.
    StreamDeclaration s;
    s.id          = StringCRC("UnusedStream");
    s.kind        = StringCRC("EventStream");
    s.fromPU      = StringCRC("MainPU");
    s.toPU        = StringCRC("MainPU");
    s.multiWriter = false;
    manifest.streams.Add(s);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(validator.HasErrors())
        << "ORPHAN_STREAM should be a warning, not an error";
    EXPECT_TRUE(validator.HasWarnings());
    EXPECT_TRUE(HasCode(validator.GetResults(), "ORPHAN_STREAM", ValidationSeverity::kWarning))
        << "Expected ORPHAN_STREAM warning for unwritten stream";
}
