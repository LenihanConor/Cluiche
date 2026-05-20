#include <gtest/gtest.h>
#include <DiaApplicationEditor/V2/ManifestValidator.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

// ==============================================================================
// Helpers
// ==============================================================================

static ManifestEditorState MakeValid()
{
    ManifestEditorState s;
    s.hasManifest = true;

    StageDeclaration sd;
    sd.name = StringCRC("Boot");
    s.manifest.stages.Add(sd);
    s.manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId = StringCRC("MainPU");
    pu.frequencyHz = 30.f;
    pu.dedicatedThread = false;

    ModuleDeclaration m;
    m.instanceId = StringCRC("ModA");
    m.typeId     = StringCRC("ModA");
    m.stages.Add(StringCRC("all"));
    pu.modules.Add(m);

    s.manifest.processingUnits.Add(pu);
    return s;
}

static const ValidationIssue* FindIssue(const ValidationResult& r, ValidationRuleId rule)
{
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
        if (r.issues[i].ruleId == rule) return &r.issues[i];
    return nullptr;
}

// ==============================================================================
// DependencyCycle
// ==============================================================================

TEST(ManifestValidator, DependencyCycle_NoCycle_NoIssue)
{
    ManifestEditorState s = MakeValid();
    // Add ModB that depends on ModA — no cycle
    ModuleDeclaration modB;
    modB.instanceId = StringCRC("ModB");
    modB.typeId     = StringCRC("ModB");
    modB.stages.Add(StringCRC("all"));
    modB.dependencies.Add(StringCRC("ModA"));
    s.manifest.processingUnits[0].modules.Add(modB);

    ValidationResult r = ManifestValidator::Validate(s);
    EXPECT_EQ(r.ErrorCount(), 0u);
}

TEST(ManifestValidator, DependencyCycle_Cycle_HasError)
{
    ManifestEditorState s = MakeValid();
    // ModA depends on ModB, ModB depends on ModA
    s.manifest.processingUnits[0].modules[0].dependencies.Add(StringCRC("ModB"));

    ModuleDeclaration modB;
    modB.instanceId = StringCRC("ModB");
    modB.typeId     = StringCRC("ModB");
    modB.stages.Add(StringCRC("all"));
    modB.dependencies.Add(StringCRC("ModA"));
    s.manifest.processingUnits[0].modules.Add(modB);

    ValidationResult r = ManifestValidator::Validate(s);
    EXPECT_TRUE(r.HasErrors());
    bool foundCycle = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::DependencyCycle)
        {
            foundCycle = true;
            break;
        }
    }
    EXPECT_TRUE(foundCycle);
}

// ==============================================================================
// OrphanModule
// ==============================================================================

TEST(ManifestValidator, OrphanModule_StagesPresent_NoIssue)
{
    ManifestEditorState s = MakeValid();
    // ModA already has "all" stage — no orphan

    ValidationResult r = ManifestValidator::Validate(s);
    bool foundOrphan = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::OrphanModule)
        {
            foundOrphan = true;
            break;
        }
    }
    EXPECT_FALSE(foundOrphan);
}

TEST(ManifestValidator, OrphanModule_NoStages_HasWarning)
{
    ManifestEditorState s = MakeValid();
    // Clear stages on ModA
    s.manifest.processingUnits[0].modules[0].stages.RemoveAll();

    ValidationResult r = ManifestValidator::Validate(s);
    bool foundOrphan = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::OrphanModule &&
            r.issues[i].severity == ValidationSeverity::Warning)
        {
            foundOrphan = true;
            break;
        }
    }
    EXPECT_TRUE(foundOrphan);
}

// ==============================================================================
// UnknownStreamInReads
// ==============================================================================

TEST(ManifestValidator, UnknownStreamInReads_StreamExists_NoIssue)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("InputEvents");
    stream.payloadType = StringCRC("InputEvent");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);

    // ModA reads it, and also writes it so we don't get orphan warnings
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("InputEvents"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("InputEvents"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::UnknownStreamInReads)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, UnknownStreamInReads_StreamMissing_HasError)
{
    ManifestEditorState s = MakeValid();
    // Module reads a stream that doesn't exist
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("GhostStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::UnknownStreamInReads)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(r.HasErrors());
}

// ==============================================================================
// UnknownStreamInWrites
// ==============================================================================

TEST(ManifestValidator, UnknownStreamInWrites_StreamExists_NoIssue)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("OutputEvents");
    stream.payloadType = StringCRC("OutputEvent");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);

    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("OutputEvents"));
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("OutputEvents"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::UnknownStreamInWrites)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, UnknownStreamInWrites_StreamMissing_HasError)
{
    ManifestEditorState s = MakeValid();
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("GhostStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::UnknownStreamInWrites)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(r.HasErrors());
}

// ==============================================================================
// OrphanReaderStream
// ==============================================================================

TEST(ManifestValidator, OrphanReaderStream_HasReader_NoIssue)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("DataStream");
    stream.payloadType = StringCRC("Data");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);

    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("DataStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("DataStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::OrphanReaderStream)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, OrphanReaderStream_NoReader_HasWarning)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("UnreadStream");
    stream.payloadType = StringCRC("Data");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    // Nobody reads UnreadStream; give it a writer to isolate the reader orphan
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("UnreadStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::OrphanReaderStream)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ==============================================================================
// OrphanWriterStream
// ==============================================================================

TEST(ManifestValidator, OrphanWriterStream_HasWriter_NoIssue)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("EventBus");
    stream.payloadType = StringCRC("Event");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);

    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("EventBus"));
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("EventBus"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::OrphanWriterStream)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, OrphanWriterStream_NoWriter_HasWarning)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("UnwrittenStream");
    stream.payloadType = StringCRC("Data");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    // Give it a reader to isolate the writer orphan
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("UnwrittenStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::OrphanWriterStream)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ==============================================================================
// PayloadTypeMissing
// ==============================================================================

TEST(ManifestValidator, PayloadTypeMissing_PayloadSet_NoIssue)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("TypableStream");
    stream.payloadType = StringCRC("SomePayload");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("TypableStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("TypableStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::PayloadTypeMissing)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, PayloadTypeMissing_EmptyPayload_HasWarning)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("UntypedStream");
    // payloadType left default (StringCRC())
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("UntypedStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("UntypedStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::PayloadTypeMissing &&
            r.issues[i].severity == ValidationSeverity::Warning)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ==============================================================================
// StageRefInvalid
// ==============================================================================

TEST(ManifestValidator, StageRefInvalid_ValidStageRef_NoIssue)
{
    ManifestEditorState s = MakeValid();
    // ModA uses "all" which is always valid; add a module using the declared "Boot" stage
    ModuleDeclaration modB;
    modB.instanceId = StringCRC("ModB");
    modB.typeId     = StringCRC("ModB");
    modB.stages.Add(StringCRC("Boot"));
    s.manifest.processingUnits[0].modules.Add(modB);

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::StageRefInvalid)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, StageRefInvalid_UnknownStageRef_HasError)
{
    ManifestEditorState s = MakeValid();
    ModuleDeclaration modB;
    modB.instanceId = StringCRC("ModB");
    modB.typeId     = StringCRC("ModB");
    modB.stages.Add(StringCRC("NonExistentStage"));
    s.manifest.processingUnits[0].modules.Add(modB);

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::StageRefInvalid)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(r.HasErrors());
}

// ==============================================================================
// DuplicateInstanceId
// ==============================================================================

TEST(ManifestValidator, DuplicateInstanceId_UniqueIds_NoIssue)
{
    ManifestEditorState s = MakeValid();
    // Add a second PU with a different module
    ProcessingUnitDeclaration pu2;
    pu2.instanceId = StringCRC("SecondPU");
    pu2.frequencyHz = 30.f;

    ModuleDeclaration modB;
    modB.instanceId = StringCRC("ModB");
    modB.typeId     = StringCRC("ModB");
    modB.stages.Add(StringCRC("all"));
    pu2.modules.Add(modB);
    s.manifest.processingUnits.Add(pu2);

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::DuplicateInstanceId)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, DuplicateInstanceId_SameIdAcrossPUs_HasError)
{
    ManifestEditorState s = MakeValid();
    // Add a second PU with a module sharing the same instanceId as ModA
    ProcessingUnitDeclaration pu2;
    pu2.instanceId = StringCRC("SecondPU");
    pu2.frequencyHz = 30.f;

    ModuleDeclaration modDup;
    modDup.instanceId = StringCRC("ModA"); // duplicate!
    modDup.typeId     = StringCRC("ModA");
    modDup.stages.Add(StringCRC("all"));
    pu2.modules.Add(modDup);
    s.manifest.processingUnits.Add(pu2);

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::DuplicateInstanceId)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(r.HasErrors());
}

// ==============================================================================
// InitialStageInvalid
// ==============================================================================

TEST(ManifestValidator, InitialStageInvalid_ValidInitialStage_NoIssue)
{
    ManifestEditorState s = MakeValid();
    // MakeValid() already sets initialStage = "Boot" and stages has "Boot"

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::InitialStageInvalid)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, InitialStageInvalid_MissingInitialStage_HasError)
{
    ManifestEditorState s = MakeValid();
    s.manifest.initialStage = StringCRC("NoSuchStage");

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::InitialStageInvalid)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(r.HasErrors());
}

// ==============================================================================
// StreamPUInvalid
// ==============================================================================

TEST(ManifestValidator, StreamPUInvalid_ValidPURefs_NoIssue)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("ConnStream");
    stream.payloadType = StringCRC("Payload");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("ConnStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("ConnStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::StreamPUInvalid)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, StreamPUInvalid_UnknownFromPU_HasError)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("BadStream");
    stream.payloadType = StringCRC("Payload");
    stream.fromPU      = StringCRC("GhostPU"); // not in manifest
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("BadStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("BadStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::StreamPUInvalid)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(r.HasErrors());
}

// ==============================================================================
// StreamSelfLoop
// ==============================================================================

TEST(ManifestValidator, StreamSelfLoop_DifferentPUs_NoIssue)
{
    ManifestEditorState s = MakeValid();

    ProcessingUnitDeclaration pu2;
    pu2.instanceId   = StringCRC("SecondPU");
    pu2.frequencyHz  = 30.f;
    ModuleDeclaration modB;
    modB.instanceId  = StringCRC("ModB");
    modB.typeId      = StringCRC("ModB");
    modB.stages.Add(StringCRC("all"));
    pu2.modules.Add(modB);
    s.manifest.processingUnits.Add(pu2);

    StreamDeclaration stream;
    stream.id          = StringCRC("CrossStream");
    stream.payloadType = StringCRC("Payload");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("SecondPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("CrossStream"));
    s.manifest.processingUnits[1].modules[0].reads.Add(StringCRC("CrossStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::StreamSelfLoop)
        {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST(ManifestValidator, StreamSelfLoop_SamePU_HasError)
{
    ManifestEditorState s = MakeValid();

    StreamDeclaration stream;
    stream.id          = StringCRC("LoopStream");
    stream.payloadType = StringCRC("Payload");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU"); // self-loop
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("LoopStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("LoopStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    bool found = false;
    for (unsigned int i = 0; i < r.issues.Size(); ++i)
    {
        if (r.issues[i].ruleId == ValidationRuleId::StreamSelfLoop)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(r.HasErrors());
}

// ==============================================================================
// Target and SuggestedCommand Tests
// ==============================================================================

TEST(ManifestValidator, DependencyCycle_Targets_PU)
{
    ManifestEditorState s = MakeValid();
    s.manifest.processingUnits[0].modules[0].dependencies.Add(StringCRC("ModB"));

    ModuleDeclaration modB;
    modB.instanceId = StringCRC("ModB");
    modB.typeId     = StringCRC("ModB");
    modB.stages.Add(StringCRC("all"));
    modB.dependencies.Add(StringCRC("ModA"));
    s.manifest.processingUnits[0].modules.Add(modB);

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::DependencyCycle);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::PU);
    EXPECT_STREQ(iss->targetPuId, "MainPU");
    EXPECT_STREQ(iss->targetModuleId, "");
    EXPECT_STREQ(iss->suggestedActionLabel, "");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "");
}

TEST(ManifestValidator, OrphanModule_Targets_Module_AndFix)
{
    ManifestEditorState s = MakeValid();
    s.manifest.processingUnits[0].modules[0].stages.RemoveAll();

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::OrphanModule);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Module);
    EXPECT_STREQ(iss->targetPuId, "MainPU");
    EXPECT_STREQ(iss->targetModuleId, "ModA");
    EXPECT_STREQ(iss->suggestedActionLabel, "Assign to 'all' stages");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "SetModuleStages");
    EXPECT_STREQ(iss->suggestedCommand.puId, "MainPU");
    EXPECT_STREQ(iss->suggestedCommand.instanceId, "ModA");
    EXPECT_STREQ(iss->suggestedCommand.stagesCSV, "all");
}

TEST(ManifestValidator, UnknownStreamInReads_Targets_AndFix)
{
    ManifestEditorState s = MakeValid();
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("GhostStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::UnknownStreamInReads);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Module);
    EXPECT_STREQ(iss->targetPuId, "MainPU");
    EXPECT_STREQ(iss->targetModuleId, "ModA");
    EXPECT_STREQ(iss->targetStreamId, "GhostStream");
    EXPECT_STREQ(iss->suggestedActionLabel, "Remove read");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "RemoveModuleRead");
    EXPECT_STREQ(iss->suggestedCommand.puId, "MainPU");
    EXPECT_STREQ(iss->suggestedCommand.instanceId, "ModA");
    EXPECT_STREQ(iss->suggestedCommand.streamId, "GhostStream");
}

TEST(ManifestValidator, UnknownStreamInWrites_Targets_AndFix)
{
    ManifestEditorState s = MakeValid();
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("GhostStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::UnknownStreamInWrites);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Module);
    EXPECT_STREQ(iss->targetPuId, "MainPU");
    EXPECT_STREQ(iss->targetModuleId, "ModA");
    EXPECT_STREQ(iss->targetStreamId, "GhostStream");
    EXPECT_STREQ(iss->suggestedActionLabel, "Remove write");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "RemoveModuleWrite");
    EXPECT_STREQ(iss->suggestedCommand.puId, "MainPU");
    EXPECT_STREQ(iss->suggestedCommand.instanceId, "ModA");
    EXPECT_STREQ(iss->suggestedCommand.streamId, "GhostStream");
}

TEST(ManifestValidator, OrphanReaderStream_Targets_AndFix)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("UnreadStream");
    stream.payloadType = StringCRC("Data");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("UnreadStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::OrphanReaderStream);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Stream);
    EXPECT_STREQ(iss->targetStreamId, "UnreadStream");
    EXPECT_STREQ(iss->suggestedActionLabel, "Remove stream");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "RemoveStream");
    EXPECT_STREQ(iss->suggestedCommand.streamId, "UnreadStream");
}

TEST(ManifestValidator, OrphanWriterStream_Targets_AndFix)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("UnwrittenStream");
    stream.payloadType = StringCRC("Data");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("UnwrittenStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::OrphanWriterStream);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Stream);
    EXPECT_STREQ(iss->targetStreamId, "UnwrittenStream");
    EXPECT_STREQ(iss->suggestedActionLabel, "Remove stream");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "RemoveStream");
    EXPECT_STREQ(iss->suggestedCommand.streamId, "UnwrittenStream");
}

TEST(ManifestValidator, PayloadTypeMissing_Targets_StreamOnly)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id = StringCRC("UntypedStream");
    stream.fromPU = StringCRC("MainPU");
    stream.toPU = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("UntypedStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("UntypedStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::PayloadTypeMissing);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Stream);
    EXPECT_STREQ(iss->targetStreamId, "UntypedStream");
    EXPECT_STREQ(iss->suggestedActionLabel, "");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "");
}

TEST(ManifestValidator, StageRefInvalid_Targets_AndFix_StagesCSV)
{
    ManifestEditorState s = MakeValid();
    s.manifest.processingUnits[0].modules[0].stages.RemoveAll();
    s.manifest.processingUnits[0].modules[0].stages.Add(StringCRC("all"));
    s.manifest.processingUnits[0].modules[0].stages.Add(StringCRC("GhostStage"));

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::StageRefInvalid);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Module);
    EXPECT_STREQ(iss->targetPuId, "MainPU");
    EXPECT_STREQ(iss->targetModuleId, "ModA");
    EXPECT_STREQ(iss->suggestedActionLabel, "Remove invalid stage ref");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "SetModuleStages");
    EXPECT_STREQ(iss->suggestedCommand.puId, "MainPU");
    EXPECT_STREQ(iss->suggestedCommand.instanceId, "ModA");
    EXPECT_STREQ(iss->suggestedCommand.stagesCSV, "all");
}

TEST(ManifestValidator, DuplicateInstanceId_Targets_Module_NoFix)
{
    ManifestEditorState s = MakeValid();
    ProcessingUnitDeclaration pu2;
    pu2.instanceId = StringCRC("SecondPU");
    pu2.frequencyHz = 30.f;

    ModuleDeclaration modDup;
    modDup.instanceId = StringCRC("ModA"); // duplicate!
    modDup.typeId     = StringCRC("ModA");
    modDup.stages.Add(StringCRC("all"));
    pu2.modules.Add(modDup);
    s.manifest.processingUnits.Add(pu2);

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::DuplicateInstanceId);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Module);
    EXPECT_STREQ(iss->targetPuId, "SecondPU");
    EXPECT_STREQ(iss->targetModuleId, "ModA");
    EXPECT_STREQ(iss->suggestedActionLabel, "");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "");
}

TEST(ManifestValidator, InitialStageInvalid_Targets_None)
{
    ManifestEditorState s = MakeValid();
    s.manifest.initialStage = StringCRC("NoSuchStage");

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::InitialStageInvalid);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::None);
    EXPECT_STREQ(iss->targetPuId, "");
    EXPECT_STREQ(iss->targetModuleId, "");
    EXPECT_STREQ(iss->targetStreamId, "");
    EXPECT_STREQ(iss->suggestedActionLabel, "");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "");
}

TEST(ManifestValidator, StreamPUInvalid_Targets_Stream)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("BadStream");
    stream.payloadType = StringCRC("Payload");
    stream.fromPU      = StringCRC("GhostPU"); // not in manifest
    stream.toPU        = StringCRC("MainPU");
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("BadStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("BadStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::StreamPUInvalid);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Stream);
    EXPECT_STREQ(iss->targetStreamId, "BadStream");
    EXPECT_STREQ(iss->suggestedActionLabel, "");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "");
}

TEST(ManifestValidator, StreamSelfLoop_Targets_Stream)
{
    ManifestEditorState s = MakeValid();
    StreamDeclaration stream;
    stream.id          = StringCRC("LoopStream");
    stream.payloadType = StringCRC("Payload");
    stream.fromPU      = StringCRC("MainPU");
    stream.toPU        = StringCRC("MainPU"); // self-loop
    s.manifest.streams.Add(stream);
    s.manifest.processingUnits[0].modules[0].reads.Add(StringCRC("LoopStream"));
    s.manifest.processingUnits[0].modules[0].writes.Add(StringCRC("LoopStream"));

    ValidationResult r = ManifestValidator::Validate(s);
    const ValidationIssue* iss = FindIssue(r, ValidationRuleId::StreamSelfLoop);
    ASSERT_NE(iss, nullptr);
    EXPECT_EQ(iss->targetKind, ValidationTargetKind::Stream);
    EXPECT_STREQ(iss->targetStreamId, "LoopStream");
    EXPECT_STREQ(iss->suggestedActionLabel, "");
    EXPECT_STREQ(iss->suggestedCommand.commandType, "");
}
