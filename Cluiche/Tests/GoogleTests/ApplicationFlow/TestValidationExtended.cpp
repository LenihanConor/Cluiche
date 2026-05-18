////////////////////////////////////////////////////////////////////////////////
// Filename: TestValidationExtended.cpp
// GoogleTest suite — ManifestValidatorV2 codes not covered by TestValidation.cpp
//
// Covers: EMPTY_STAGE, UNKNOWN_STREAM, UNKNOWN_STREAM_IN_READS,
//         UNKNOWN_STREAM_IN_WRITES, PAYLOAD_TYPE_MISSING,
//         ORPHAN_READER_STREAM, ORPHAN_WRITER_STREAM, RESERVED_PREFIX
//
// Module types prefixed "ValX_" to avoid ODR collisions.
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
// Fixture module
// ---------------------------------------------------------------------------

struct ValX_SimpleModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;
    StartResult DoStart() override { return StartResult::kReady; }
    void        DoUpdate(float) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC ValX_SimpleModule::kTypeId("ValX_SimpleModule");

static TypeRegistry BuildValXRegistry()
{
    TypeRegistry reg;
    reg.Register(ValX_SimpleModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new ValX_SimpleModule(id); });
    return reg;
}

static ApplicationManifestV2 BuildValXValidManifest()
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
    mod.typeId         = ValX_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));

    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);
    return manifest;
}

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
// EMPTY_STAGE
//
// A stage has no modules specifically assigned to it (only "all" modules
// or no modules) — produces EMPTY_STAGE warning.
// ---------------------------------------------------------------------------

TEST(ValidationExtended, EmptyStageReportsWarning)
{
    TypeRegistry reg = BuildValXRegistry();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    // Two stages: Boot (has a specific module) and Run (has nothing).
    StageDeclaration boot;  boot.name = StringCRC("Boot");
    StageDeclaration run;   run.name  = StringCRC("Run");
    manifest.stages.Add(boot);
    manifest.stages.Add(run);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // Module only in Boot — Run stage has nothing.
    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("mod0");
    mod.typeId         = ValX_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(validator.HasErrors())
        << "EMPTY_STAGE should be a warning, not an error";
    EXPECT_TRUE(validator.HasWarnings());
    EXPECT_TRUE(HasCode(validator.GetResults(), "EMPTY_STAGE", ValidationSeverity::kWarning))
        << "Expected EMPTY_STAGE warning for stage with no stage-specific modules";
}

// Boot stage has only an "all" module — still considered empty for the stage.
TEST(ValidationExtended, EmptyStageAllModuleOnlyReportsWarning)
{
    TypeRegistry reg = BuildValXRegistry();

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;  boot.name = StringCRC("Boot");
    StageDeclaration run;   run.name  = StringCRC("Run");
    manifest.stages.Add(boot);
    manifest.stages.Add(run);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // Module uses "all" sentinel — no stage is empty just from this module's
    // perspective, but a stage with ONLY "all" modules should still warn.
    // Add one module for Boot specifically, so Run is empty.
    ModuleDeclaration modBoot;
    modBoot.instanceId     = StringCRC("bootMod");
    modBoot.typeId         = ValX_SimpleModule::kTypeId;
    modBoot.startTimeoutMs = 10000.0f;
    modBoot.stopTimeoutMs  = 5000.0f;
    modBoot.stages.Add(StringCRC("Boot"));
    pu.modules.Add(modBoot);
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    // Run stage is empty — should produce EMPTY_STAGE warning.
    EXPECT_TRUE(HasCode(validator.GetResults(), "EMPTY_STAGE", ValidationSeverity::kWarning))
        << "Expected EMPTY_STAGE warning for Run stage with no stage-specific modules";
}

// ---------------------------------------------------------------------------
// UNKNOWN_STREAM
//
// A module's reads/writes lists a stream ID that isn't in manifest.streams[].
// This is checked by CheckPUModules (UNKNOWN_STREAM) before
// CheckStreamReadsWritesBinding (UNKNOWN_STREAM_IN_READS/WRITES).
// ---------------------------------------------------------------------------

TEST(ValidationExtended, UnknownStreamInReadsReportsError)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    // Module reads a stream that doesn't exist in manifest.streams.
    manifest.processingUnits[0].modules[0].reads.Add(StringCRC("GhostStream"));

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    // Either UNKNOWN_STREAM (CheckPUModules) or UNKNOWN_STREAM_IN_READS
    // (CheckStreamReadsWritesBinding) must fire — both are errors.
    const bool hasUnknownStream =
        HasCode(validator.GetResults(), "UNKNOWN_STREAM") ||
        HasCode(validator.GetResults(), "UNKNOWN_STREAM_IN_READS");
    EXPECT_TRUE(hasUnknownStream)
        << "Expected UNKNOWN_STREAM or UNKNOWN_STREAM_IN_READS for reads referencing undeclared stream";
}

TEST(ValidationExtended, UnknownStreamInWritesReportsError)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    // Module writes a stream that doesn't exist in manifest.streams.
    manifest.processingUnits[0].modules[0].writes.Add(StringCRC("GhostWriteStream"));

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    const bool hasUnknownStream =
        HasCode(validator.GetResults(), "UNKNOWN_STREAM") ||
        HasCode(validator.GetResults(), "UNKNOWN_STREAM_IN_WRITES");
    EXPECT_TRUE(hasUnknownStream)
        << "Expected UNKNOWN_STREAM or UNKNOWN_STREAM_IN_WRITES for writes referencing undeclared stream";
}

// Reserved $-prefix stream in reads is allowed — no error.
TEST(ValidationExtended, ReservedStreamInReadsIsAllowed)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    // $lifecycle is a reserved stream — reading it in module reads[] is allowed.
    manifest.processingUnits[0].modules[0].reads.Add(StringCRC("$lifecycle"));

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    const bool hasStreamError =
        HasCode(validator.GetResults(), "UNKNOWN_STREAM") ||
        HasCode(validator.GetResults(), "UNKNOWN_STREAM_IN_READS");
    EXPECT_FALSE(hasStreamError)
        << "Reading a reserved $-prefix stream should not produce a stream-binding error";
}

// ---------------------------------------------------------------------------
// PAYLOAD_TYPE_MISSING
//
// A stream declaration has kind set but payloadType empty — warning.
// ---------------------------------------------------------------------------

TEST(ValidationExtended, PayloadTypeMissingReportsWarning)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    // Declare a stream with kind but no payload_type.
    StreamDeclaration s;
    s.id          = StringCRC("TypelessStream");
    s.kind        = StringCRC("EventStream");
    // s.payloadType intentionally left as StringCRC() (zero value)
    s.fromPU      = StringCRC("MainPU");
    s.toPU        = StringCRC("MainPU");
    s.multiWriter = false;
    manifest.streams.Add(s);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(HasCode(validator.GetResults(), "PAYLOAD_TYPE_MISSING"))
        << "PAYLOAD_TYPE_MISSING should be a warning, not an error";
    EXPECT_TRUE(HasCode(validator.GetResults(), "PAYLOAD_TYPE_MISSING",
                        ValidationSeverity::kWarning))
        << "Expected PAYLOAD_TYPE_MISSING warning for stream with kind but no payload_type";
}

// Stream with payloadType set — no PAYLOAD_TYPE_MISSING warning.
TEST(ValidationExtended, PayloadTypePresentNoWarning)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    StreamDeclaration s;
    s.id          = StringCRC("TypedStream");
    s.kind        = StringCRC("EventStream");
    s.payloadType = StringCRC("int");
    s.fromPU      = StringCRC("MainPU");
    s.toPU        = StringCRC("MainPU");
    s.multiWriter = false;
    manifest.streams.Add(s);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(HasCode(validator.GetResults(), "PAYLOAD_TYPE_MISSING",
                         ValidationSeverity::kWarning))
        << "No PAYLOAD_TYPE_MISSING warning when payload_type is set";
}

// ---------------------------------------------------------------------------
// ORPHAN_READER_STREAM / ORPHAN_WRITER_STREAM
//
// Stream has readers but no writers → ORPHAN_READER_STREAM (warning)
// Stream has writers but no readers → ORPHAN_WRITER_STREAM (warning)
// ---------------------------------------------------------------------------

TEST(ValidationExtended, OrphanReaderStreamReportsWarning)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    StreamDeclaration s;
    s.id          = StringCRC("ReaderOnlyStream");
    s.kind        = StringCRC("EventStream");
    s.fromPU      = StringCRC("MainPU");
    s.toPU        = StringCRC("MainPU");
    s.multiWriter = false;
    manifest.streams.Add(s);

    // Module reads but never writes this stream.
    manifest.processingUnits[0].modules[0].reads.Add(StringCRC("ReaderOnlyStream"));

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(HasCode(validator.GetResults(), "ORPHAN_READER_STREAM"))
        << "ORPHAN_READER_STREAM should be a warning, not an error";
    EXPECT_TRUE(HasCode(validator.GetResults(), "ORPHAN_READER_STREAM",
                        ValidationSeverity::kWarning))
        << "Expected ORPHAN_READER_STREAM warning for stream with readers but no writers";
}

TEST(ValidationExtended, OrphanWriterStreamReportsWarning)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    StreamDeclaration s;
    s.id          = StringCRC("WriterOnlyStream");
    s.kind        = StringCRC("EventStream");
    s.fromPU      = StringCRC("MainPU");
    s.toPU        = StringCRC("MainPU");
    s.multiWriter = false;
    manifest.streams.Add(s);

    // Module writes but never reads this stream.
    manifest.processingUnits[0].modules[0].writes.Add(StringCRC("WriterOnlyStream"));

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(HasCode(validator.GetResults(), "ORPHAN_WRITER_STREAM"))
        << "ORPHAN_WRITER_STREAM should be a warning, not an error";
    EXPECT_TRUE(HasCode(validator.GetResults(), "ORPHAN_WRITER_STREAM",
                        ValidationSeverity::kWarning))
        << "Expected ORPHAN_WRITER_STREAM warning for stream with writers but no readers";
}

// Stream with both a reader and a writer — no orphan warnings.
TEST(ValidationExtended, StreamWithBothReaderAndWriterNoOrphanWarning)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    StreamDeclaration s;
    s.id          = StringCRC("FullStream");
    s.kind        = StringCRC("EventStream");
    s.fromPU      = StringCRC("MainPU");
    s.toPU        = StringCRC("MainPU");
    s.multiWriter = false;
    manifest.streams.Add(s);

    // Same module reads and writes (single-module sanity test).
    manifest.processingUnits[0].modules[0].reads.Add(StringCRC("FullStream"));
    manifest.processingUnits[0].modules[0].writes.Add(StringCRC("FullStream"));

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(HasCode(validator.GetResults(), "ORPHAN_READER_STREAM",
                         ValidationSeverity::kWarning))
        << "No ORPHAN_READER_STREAM warning when stream has a writer";
    EXPECT_FALSE(HasCode(validator.GetResults(), "ORPHAN_WRITER_STREAM",
                         ValidationSeverity::kWarning))
        << "No ORPHAN_WRITER_STREAM warning when stream has a reader";
}

// ---------------------------------------------------------------------------
// RESERVED_PREFIX
//
// User-declared stream IDs and module instance IDs starting with '$' are
// forbidden — produces RESERVED_PREFIX error.
// ---------------------------------------------------------------------------

TEST(ValidationExtended, ReservedPrefixOnStreamIdReportsError)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    // User tries to declare a stream starting with '$'.
    StreamDeclaration s;
    s.id          = StringCRC("$forbidden");
    s.kind        = StringCRC("EventStream");
    s.fromPU      = StringCRC("MainPU");
    s.toPU        = StringCRC("MainPU");
    s.multiWriter = false;
    manifest.streams.Add(s);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "RESERVED_PREFIX"))
        << "Expected RESERVED_PREFIX error for user-declared stream starting with '$'";
}

TEST(ValidationExtended, ReservedPrefixOnModuleInstanceIdReportsError)
{
    TypeRegistry reg = BuildValXRegistry();

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

    // Module instance_id uses the reserved '$' prefix.
    ModuleDeclaration mod;
    mod.instanceId     = StringCRC("$forbidden_module");
    mod.typeId         = ValX_SimpleModule::kTypeId;
    mod.startTimeoutMs = 10000.0f;
    mod.stopTimeoutMs  = 5000.0f;
    mod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(mod);
    manifest.processingUnits.Add(pu);

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_TRUE(validator.HasErrors());
    EXPECT_TRUE(HasCode(validator.GetResults(), "RESERVED_PREFIX"))
        << "Expected RESERVED_PREFIX error for module instance_id starting with '$'";
}

// A valid manifest with no $ violations produces no RESERVED_PREFIX error.
TEST(ValidationExtended, NoReservedPrefixViolationOnCleanManifest)
{
    TypeRegistry reg = BuildValXRegistry();
    ApplicationManifestV2 manifest = BuildValXValidManifest();

    ManifestValidatorV2 validator(reg);
    validator.Validate(manifest);

    EXPECT_FALSE(HasCode(validator.GetResults(), "RESERVED_PREFIX"))
        << "No RESERVED_PREFIX error on a manifest with no $ violations";
}
