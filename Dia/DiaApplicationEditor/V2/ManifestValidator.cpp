#include <DiaApplicationEditor/V2/ManifestValidator.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <stdio.h>
#include <string.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

namespace Dia { namespace ApplicationFlow { namespace Editor {

// ---------------------------------------------------------------------------
// ValidationResult
// ---------------------------------------------------------------------------

bool ValidationResult::HasErrors() const
{
    for (unsigned int i = 0; i < issues.Size(); ++i)
    {
        if (issues[i].severity == ValidationSeverity::Error)
            return true;
    }
    return false;
}

bool ValidationResult::HasWarnings() const
{
    for (unsigned int i = 0; i < issues.Size(); ++i)
    {
        if (issues[i].severity == ValidationSeverity::Warning)
            return true;
    }
    return false;
}

unsigned int ValidationResult::ErrorCount() const
{
    unsigned int count = 0;
    for (unsigned int i = 0; i < issues.Size(); ++i)
    {
        if (issues[i].severity == ValidationSeverity::Error)
            ++count;
    }
    return count;
}

unsigned int ValidationResult::WarningCount() const
{
    unsigned int count = 0;
    for (unsigned int i = 0; i < issues.Size(); ++i)
    {
        if (issues[i].severity == ValidationSeverity::Warning)
            ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void AddIssue(ValidationResult& result, ValidationRuleId ruleId,
                     ValidationSeverity severity, const char* message)
{
    if (result.issues.IsFull())
        return;
    ValidationIssue issue;
    issue.ruleId   = ruleId;
    issue.severity = severity;
    strncpy(issue.message, message, sizeof(issue.message) - 1);
    issue.message[sizeof(issue.message) - 1] = '\0';
    result.issues.Add(issue);
}

static bool StreamIdExists(const ApplicationManifestV2& manifest, StringCRC id)
{
    for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
    {
        if (manifest.streams[i].id == id)
            return true;
    }
    return false;
}

static bool StageNameExists(const ApplicationManifestV2& manifest, StringCRC name)
{
    for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
    {
        if (manifest.stages[i].name == name)
            return true;
    }
    return false;
}

static bool PUIdExists(const ApplicationManifestV2& manifest, StringCRC id)
{
    for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
    {
        if (manifest.processingUnits[i].instanceId == id)
            return true;
    }
    return false;
}

// DFS cycle detection for a single PU's module dependency graph.
// Returns true if a cycle was found starting at node `start`.
// visited[i] and inStack[i] index into the modules array of the PU.
static bool DFSHasCycle(const ProcessingUnitDeclaration& pu,
                        unsigned int node,
                        bool* visited,
                        bool* inStack)
{
    visited[node] = true;
    inStack[node] = true;

    const ModuleDeclaration& mod = pu.modules[node];
    for (unsigned int d = 0; d < mod.dependencies.Size(); ++d)
    {
        StringCRC depId = mod.dependencies[d];
        // Find the index of this dependency within the PU
        for (unsigned int j = 0; j < pu.modules.Size(); ++j)
        {
            if (pu.modules[j].instanceId == depId)
            {
                if (!visited[j])
                {
                    if (DFSHasCycle(pu, j, visited, inStack))
                        return true;
                }
                else if (inStack[j])
                {
                    return true;
                }
                break;
            }
        }
    }

    inStack[node] = false;
    return false;
}

// ---------------------------------------------------------------------------
// Validate
// ---------------------------------------------------------------------------

ValidationResult ManifestValidator::Validate(const ManifestEditorState& state)
{
    ValidationResult result;

    if (!state.hasManifest)
        return result;

    const ApplicationManifestV2& manifest = state.manifest;
    char msg[256];

    // ------------------------------------------------------------------
    // DEPENDENCY_CYCLE: per-PU DFS cycle detection
    // ------------------------------------------------------------------
    for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
    {
        const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
        const unsigned int moduleCount = pu.modules.Size();
        if (moduleCount == 0)
            continue;

        static constexpr unsigned int kMaxModulesPerPU = 32;
        bool visited[kMaxModulesPerPU] = {};
        bool inStack[kMaxModulesPerPU] = {};

        const unsigned int checkCount = moduleCount < kMaxModulesPerPU ? moduleCount : kMaxModulesPerPU;
        bool cycleFound = false;
        for (unsigned int i = 0; i < checkCount && !cycleFound; ++i)
        {
            if (!visited[i])
            {
                if (DFSHasCycle(pu, i, visited, inStack))
                {
                    cycleFound = true;
                    snprintf(msg, sizeof(msg),
                             "DEPENDENCY_CYCLE: cycle detected in PU 0x%08X",
                             pu.instanceId.Value());
                    AddIssue(result, ValidationRuleId::DependencyCycle,
                             ValidationSeverity::Error, msg);
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // ORPHAN_MODULE: module has no stages listed
    // ------------------------------------------------------------------
    for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
    {
        const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
        for (unsigned int m = 0; m < pu.modules.Size(); ++m)
        {
            const ModuleDeclaration& mod = pu.modules[m];
            if (mod.stages.Size() == 0)
            {
                snprintf(msg, sizeof(msg),
                         "ORPHAN_MODULE: module 0x%08X in PU 0x%08X has no stage assignments",
                         mod.instanceId.Value(), pu.instanceId.Value());
                AddIssue(result, ValidationRuleId::OrphanModule,
                         ValidationSeverity::Warning, msg);
            }
        }
    }

    // ------------------------------------------------------------------
    // UNKNOWN_STREAM_IN_READS / UNKNOWN_STREAM_IN_WRITES
    // ------------------------------------------------------------------
    for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
    {
        const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
        for (unsigned int m = 0; m < pu.modules.Size(); ++m)
        {
            const ModuleDeclaration& mod = pu.modules[m];

            for (unsigned int r = 0; r < mod.reads.Size(); ++r)
            {
                if (!StreamIdExists(manifest, mod.reads[r]))
                {
                    snprintf(msg, sizeof(msg),
                             "UNKNOWN_STREAM_IN_READS: module 0x%08X reads stream 0x%08X which is not declared",
                             mod.instanceId.Value(), mod.reads[r].Value());
                    AddIssue(result, ValidationRuleId::UnknownStreamInReads,
                             ValidationSeverity::Error, msg);
                }
            }

            for (unsigned int w = 0; w < mod.writes.Size(); ++w)
            {
                if (!StreamIdExists(manifest, mod.writes[w]))
                {
                    snprintf(msg, sizeof(msg),
                             "UNKNOWN_STREAM_IN_WRITES: module 0x%08X writes stream 0x%08X which is not declared",
                             mod.instanceId.Value(), mod.writes[w].Value());
                    AddIssue(result, ValidationRuleId::UnknownStreamInWrites,
                             ValidationSeverity::Error, msg);
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // ORPHAN_READER_STREAM / ORPHAN_WRITER_STREAM
    // ------------------------------------------------------------------
    for (unsigned int s = 0; s < manifest.streams.Size(); ++s)
    {
        const StreamDeclaration& stream = manifest.streams[s];

        bool hasReader = false;
        bool hasWriter = false;

        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const ModuleDeclaration& mod = pu.modules[m];

                for (unsigned int r = 0; r < mod.reads.Size(); ++r)
                {
                    if (mod.reads[r] == stream.id)
                    {
                        hasReader = true;
                        break;
                    }
                }

                for (unsigned int w = 0; w < mod.writes.Size(); ++w)
                {
                    if (mod.writes[w] == stream.id)
                    {
                        hasWriter = true;
                        break;
                    }
                }

                if (hasReader && hasWriter)
                    break;
            }
            if (hasReader && hasWriter)
                break;
        }

        if (!hasReader)
        {
            snprintf(msg, sizeof(msg),
                     "ORPHAN_READER_STREAM: stream 0x%08X has no module reading it",
                     stream.id.Value());
            AddIssue(result, ValidationRuleId::OrphanReaderStream,
                     ValidationSeverity::Warning, msg);
        }

        if (!hasWriter)
        {
            snprintf(msg, sizeof(msg),
                     "ORPHAN_WRITER_STREAM: stream 0x%08X has no module writing it",
                     stream.id.Value());
            AddIssue(result, ValidationRuleId::OrphanWriterStream,
                     ValidationSeverity::Warning, msg);
        }
    }

    // ------------------------------------------------------------------
    // PAYLOAD_TYPE_MISSING
    // ------------------------------------------------------------------
    {
        const Dia::Core::StringCRC kZero;
        for (unsigned int s = 0; s < manifest.streams.Size(); ++s)
        {
            if (manifest.streams[s].payloadType == kZero)
            {
                snprintf(msg, sizeof(msg),
                         "PAYLOAD_TYPE_MISSING: stream 0x%08X has no payloadType set",
                         manifest.streams[s].id.Value());
                AddIssue(result, ValidationRuleId::PayloadTypeMissing,
                         ValidationSeverity::Warning, msg);
            }
        }
    }

    // ------------------------------------------------------------------
    // STAGE_REF_INVALID
    // ------------------------------------------------------------------
    {
        const Dia::Core::StringCRC kAll("all");
        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const ModuleDeclaration& mod = pu.modules[m];
                for (unsigned int st = 0; st < mod.stages.Size(); ++st)
                {
                    const StringCRC& stageRef = mod.stages[st];
                    if (stageRef != kAll && !StageNameExists(manifest, stageRef))
                    {
                        snprintf(msg, sizeof(msg),
                                 "STAGE_REF_INVALID: module 0x%08X references stage 0x%08X which is not declared",
                                 mod.instanceId.Value(), stageRef.Value());
                        AddIssue(result, ValidationRuleId::StageRefInvalid,
                                 ValidationSeverity::Error, msg);
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // DUPLICATE_INSTANCE_ID
    // ------------------------------------------------------------------
    {
        static constexpr unsigned int kMaxModules = 128;
        StringCRC seen[kMaxModules];
        unsigned int seenCount = 0;

        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const StringCRC& id = pu.modules[m].instanceId;
                bool duplicate = false;
                for (unsigned int k = 0; k < seenCount; ++k)
                {
                    if (seen[k] == id)
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate)
                {
                    snprintf(msg, sizeof(msg),
                             "DUPLICATE_INSTANCE_ID: module instanceId 0x%08X appears in multiple PUs",
                             id.Value());
                    AddIssue(result, ValidationRuleId::DuplicateInstanceId,
                             ValidationSeverity::Error, msg);
                }
                else if (seenCount < kMaxModules)
                {
                    seen[seenCount++] = id;
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // INITIAL_STAGE_INVALID
    // ------------------------------------------------------------------
    {
        const Dia::Core::StringCRC kZero;
        if (manifest.stages.Size() > 0)
        {
            if (manifest.initialStage == kZero)
            {
                snprintf(msg, sizeof(msg),
                         "INITIAL_STAGE_INVALID: stages are defined but initialStage is not set");
                AddIssue(result, ValidationRuleId::InitialStageInvalid,
                         ValidationSeverity::Error, msg);
            }
            else if (!StageNameExists(manifest, manifest.initialStage))
            {
                snprintf(msg, sizeof(msg),
                         "INITIAL_STAGE_INVALID: initialStage 0x%08X is not in manifest.stages",
                         manifest.initialStage.Value());
                AddIssue(result, ValidationRuleId::InitialStageInvalid,
                         ValidationSeverity::Error, msg);
            }
        }
    }

    // ------------------------------------------------------------------
    // STREAM_PU_INVALID / STREAM_SELF_LOOP
    // ------------------------------------------------------------------
    {
        const Dia::Core::StringCRC kZero;
        for (unsigned int s = 0; s < manifest.streams.Size(); ++s)
        {
            const StreamDeclaration& stream = manifest.streams[s];

            if (stream.fromPU != kZero && !PUIdExists(manifest, stream.fromPU))
            {
                snprintf(msg, sizeof(msg),
                         "STREAM_PU_INVALID: stream 0x%08X fromPU 0x%08X is not a known PU instanceId",
                         stream.id.Value(), stream.fromPU.Value());
                AddIssue(result, ValidationRuleId::StreamPUInvalid,
                         ValidationSeverity::Error, msg);
            }

            if (stream.toPU != kZero && !PUIdExists(manifest, stream.toPU))
            {
                snprintf(msg, sizeof(msg),
                         "STREAM_PU_INVALID: stream 0x%08X toPU 0x%08X is not a known PU instanceId",
                         stream.id.Value(), stream.toPU.Value());
                AddIssue(result, ValidationRuleId::StreamPUInvalid,
                         ValidationSeverity::Error, msg);
            }

            if (stream.fromPU != kZero && stream.fromPU == stream.toPU)
            {
                snprintf(msg, sizeof(msg),
                         "STREAM_SELF_LOOP: stream 0x%08X has fromPU == toPU (0x%08X)",
                         stream.id.Value(), stream.fromPU.Value());
                AddIssue(result, ValidationRuleId::StreamSelfLoop,
                         ValidationSeverity::Error, msg);
            }
        }
    }

    return result;
}

}}} // namespace Dia::ApplicationFlow::Editor
