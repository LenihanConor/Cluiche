#include "ManifestValidatorV2.h"

namespace Dia { namespace ApplicationFlow {

    //-----------------------------------------------------------------------------
    // ManifestValidatorV2
    //-----------------------------------------------------------------------------

    ManifestValidatorV2::ManifestValidatorV2(const TypeRegistry& registry)
        : mRegistry(registry)
        , mResults()
    {
    }

    void ManifestValidatorV2::Validate(const ApplicationManifestV2& manifest)
    {
        Clear();

        CheckDuplicatePUIds(manifest);
        CheckDuplicateStreamIds(manifest);
        CheckStageReferences(manifest);
        CheckStreamPUReferences(manifest);
        CheckPUModules(manifest);
        CheckOrphanModules(manifest);
        CheckOrphanStreams(manifest);
        CheckEmptyStages(manifest);
        CheckMultiWriterViolations(manifest);
        CheckStreamReadsWritesBinding(manifest);
        CheckStreamPayloadTypes(manifest);
        CheckStreamOrphanReadersWriters(manifest);
        CheckReservedPrefixViolations(manifest);
    }

    bool ManifestValidatorV2::HasErrors() const
    {
        for (unsigned int i = 0; i < mResults.Size(); ++i)
        {
            if (mResults[i].severity == ValidationSeverity::kError)
            {
                return true;
            }
        }
        return false;
    }

    bool ManifestValidatorV2::HasWarnings() const
    {
        for (unsigned int i = 0; i < mResults.Size(); ++i)
        {
            if (mResults[i].severity == ValidationSeverity::kWarning)
            {
                return true;
            }
        }
        return false;
    }

    const Dia::Core::Containers::DynamicArrayC<ValidationEntry, 64>& ManifestValidatorV2::GetResults() const
    {
        return mResults;
    }

    void ManifestValidatorV2::Clear()
    {
        mResults.RemoveAll();
    }

    void ManifestValidatorV2::AddError(const char* code, const char* message, const Dia::Core::StringCRC& entityId)
    {
        ValidationEntry entry;
        entry.severity = ValidationSeverity::kError;
        entry.code     = Dia::Core::StringCRC(code);
        entry.message  = Dia::Core::Containers::String256(message);
        entry.entityId = entityId;
        mResults.Add(entry);
    }

    void ManifestValidatorV2::AddWarning(const char* code, const char* message, const Dia::Core::StringCRC& entityId)
    {
        ValidationEntry entry;
        entry.severity = ValidationSeverity::kWarning;
        entry.code     = Dia::Core::StringCRC(code);
        entry.message  = Dia::Core::Containers::String256(message);
        entry.entityId = entityId;
        mResults.Add(entry);
    }

    //-----------------------------------------------------------------------------
    // CheckStageReferences
    //
    // Validates that initialStage, autoStages entries, and all module stage
    // references point to declared stage names.  The sentinel StringCRC("all")
    // is always valid for module stages.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckStageReferences(const ApplicationManifestV2& manifest)
    {
        // Build a flat array of valid stage names from manifest.stages
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> validStages;
        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
        {
            validStages.Add(manifest.stages[i].name);
        }

        // Helper lambda-equivalent: linear search through validStages
        // Returns true if stageName is in validStages
        auto stageExists = [&validStages](const Dia::Core::StringCRC& stageName) -> bool {
            for (unsigned int i = 0; i < validStages.Size(); ++i)
            {
                if (validStages[i] == stageName)
                {
                    return true;
                }
            }
            return false;
        };

        // Check initialStage (only if non-zero)
        if (manifest.initialStage.Value() != 0)
        {
            if (!stageExists(manifest.initialStage))
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("initialStage '%s' is not declared in stages array", manifest.initialStage.AsChar());
                AddError("UNKNOWN_STAGE", msg.AsCStr(), manifest.initialStage);
            }
        }

        // Check autoStages entries
        for (unsigned int i = 0; i < manifest.autoStages.Size(); ++i)
        {
            const Dia::Core::StringCRC& stageName = manifest.autoStages[i];
            if (!stageExists(stageName))
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("autoStages[%u] '%s' is not declared in stages array", i, stageName.AsChar());
                AddError("UNKNOWN_STAGE", msg.AsCStr(), stageName);
            }
        }

        // Check module stage references across all PUs
        const Dia::Core::StringCRC kAll("all");
        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const ModuleDeclaration& mod = pu.modules[m];
                for (unsigned int s = 0; s < mod.stages.Size(); ++s)
                {
                    const Dia::Core::StringCRC& stageName = mod.stages[s];
                    // "all" sentinel is always valid
                    if (stageName == kAll)
                    {
                        continue;
                    }
                    if (!stageExists(stageName))
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' in PU '%s' references unknown stage '%s'",
                            mod.instanceId.AsChar(), pu.instanceId.AsChar(), stageName.AsChar());
                        AddError("UNKNOWN_STAGE", msg.AsCStr(), mod.instanceId);
                    }
                }
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckDuplicatePUIds
    //
    // All processing unit instance_ids must be unique.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckDuplicatePUIds(const ApplicationManifestV2& manifest)
    {
        // Linear-scan uniqueness check using a flat array of seen ids
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> seenIds;

        for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
        {
            const Dia::Core::StringCRC& puId = manifest.processingUnits[i].instanceId;

            bool found = false;
            for (unsigned int j = 0; j < seenIds.Size(); ++j)
            {
                if (seenIds[j] == puId)
                {
                    found = true;
                    break;
                }
            }

            if (found)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Duplicate processingUnit instance_id '%s' at index %u", puId.AsChar(), i);
                AddError("DUPLICATE_PU_ID", msg.AsCStr(), puId);
            }
            else
            {
                seenIds.Add(puId);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckDuplicateStreamIds
    //
    // All stream IDs must be unique.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckDuplicateStreamIds(const ApplicationManifestV2& manifest)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> seenIds;

        for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
        {
            const Dia::Core::StringCRC& streamId = manifest.streams[i].id;

            bool found = false;
            for (unsigned int j = 0; j < seenIds.Size(); ++j)
            {
                if (seenIds[j] == streamId)
                {
                    found = true;
                    break;
                }
            }

            if (found)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Duplicate stream id '%s' at index %u", streamId.AsChar(), i);
                AddError("DUPLICATE_STREAM_ID", msg.AsCStr(), streamId);
            }
            else
            {
                seenIds.Add(streamId);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckStreamPUReferences
    //
    // For each stream, checks that fromPU and toPU reference declared PU ids.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckStreamPUReferences(const ApplicationManifestV2& manifest)
    {
        // Build flat array of valid PU ids
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> puIds;
        for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
        {
            puIds.Add(manifest.processingUnits[i].instanceId);
        }

        auto puExists = [&puIds](const Dia::Core::StringCRC& id) -> bool {
            for (unsigned int i = 0; i < puIds.Size(); ++i)
            {
                if (puIds[i] == id)
                {
                    return true;
                }
            }
            return false;
        };

        for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
        {
            const StreamDeclaration& stream = manifest.streams[i];

            if (stream.fromPU.Value() != 0 && !puExists(stream.fromPU))
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' fromPU '%s' does not reference a declared processingUnit",
                    stream.id.AsChar(), stream.fromPU.AsChar());
                AddError("UNKNOWN_PU", msg.AsCStr(), stream.id);
            }

            if (stream.toPU.Value() != 0 && !puExists(stream.toPU))
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' toPU '%s' does not reference a declared processingUnit",
                    stream.id.AsChar(), stream.toPU.AsChar());
                AddError("UNKNOWN_PU", msg.AsCStr(), stream.id);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckPUModules
    //
    // Per-PU checks:
    //   - Duplicate module instance_ids within the PU
    //   - Each module type_id exists in TypeRegistry
    //   - Each dependency references an existing instance_id in the same PU
    //   - Each reads/writes entry references an existing stream id
    //   - Cycle detection via Kahn's algorithm
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckPUModules(const ApplicationManifestV2& manifest)
    {
        // Build flat array of valid stream ids
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> streamIds;
        for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
        {
            streamIds.Add(manifest.streams[i].id);
        }

        auto streamExists = [&streamIds](const Dia::Core::StringCRC& id) -> bool {
            for (unsigned int i = 0; i < streamIds.Size(); ++i)
            {
                if (streamIds[i] == id)
                {
                    return true;
                }
            }
            return false;
        };

        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];

            // --- Duplicate module instance_ids within this PU ---
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> seenModuleIds;
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const ModuleDeclaration& mod = pu.modules[m];

                bool found = false;
                for (unsigned int k = 0; k < seenModuleIds.Size(); ++k)
                {
                    if (seenModuleIds[k] == mod.instanceId)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    Dia::Core::Containers::String256 msg;
                    msg.Format("Duplicate module instance_id '%s' in processingUnit '%s'",
                        mod.instanceId.AsChar(), pu.instanceId.AsChar());
                    AddError("DUPLICATE_MODULE_ID", msg.AsCStr(), mod.instanceId);
                }
                else
                {
                    seenModuleIds.Add(mod.instanceId);
                }
            }

            // --- Per-module checks: type, deps, streams ---
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const ModuleDeclaration& mod = pu.modules[m];

                // Check type_id in TypeRegistry
                if (!mRegistry.Contains(mod.typeId))
                {
                    Dia::Core::Containers::String256 msg;
                    msg.Format("Module '%s' in PU '%s' has unknown type_id '%s'",
                        mod.instanceId.AsChar(), pu.instanceId.AsChar(), mod.typeId.AsChar());
                    AddError("UNKNOWN_TYPE", msg.AsCStr(), mod.instanceId);
                }

                // Check dependencies reference existing instance_ids in the same PU
                // and appear EARLIER in the module array than this module.  The
                // framework uses array order as startup order (forward) and reverse
                // array order as stop order; both invariants require deps-before-
                // dependents in declaration order.  A dep at a later index would
                // start AFTER its dependent (kStarting fails to find it) and be
                // torn down BEFORE its dependent (kStopping calls into freed
                // dependency resources — see ProcessingUnit::Update two-pass tick).
                for (unsigned int d = 0; d < mod.dependencies.Size(); ++d)
                {
                    const Dia::Core::StringCRC& depId = mod.dependencies[d];
                    bool depFound    = false;
                    unsigned int depIndex = 0;
                    for (unsigned int k = 0; k < pu.modules.Size(); ++k)
                    {
                        if (pu.modules[k].instanceId == depId)
                        {
                            depFound = true;
                            depIndex = k;
                            break;
                        }
                    }
                    if (!depFound)
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' in PU '%s' depends on unknown instance_id '%s'",
                            mod.instanceId.AsChar(), pu.instanceId.AsChar(), depId.AsChar());
                        AddError("UNKNOWN_DEPENDENCY", msg.AsCStr(), mod.instanceId);
                    }
                    else if (depIndex >= m)
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' in PU '%s' depends on '%s' but '%s' appears at index %u "
                                   "(must be earlier than '%s' at index %u). "
                                   "Array order is startup order — reorder the 'modules' list so dependencies come first.",
                                   mod.instanceId.AsChar(), pu.instanceId.AsChar(),
                                   depId.AsChar(), depId.AsChar(), depIndex,
                                   mod.instanceId.AsChar(), m);
                        AddError("DEPENDENCY_ORDER", msg.AsCStr(), mod.instanceId);
                    }
                }

                // Check reads references
                for (unsigned int r = 0; r < mod.reads.Size(); ++r)
                {
                    const Dia::Core::StringCRC& streamId = mod.reads[r];
                    if (!streamExists(streamId))
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' in PU '%s' reads unknown stream '%s'",
                            mod.instanceId.AsChar(), pu.instanceId.AsChar(), streamId.AsChar());
                        AddError("UNKNOWN_STREAM", msg.AsCStr(), mod.instanceId);
                    }
                }

                // Check writes references
                for (unsigned int w = 0; w < mod.writes.Size(); ++w)
                {
                    const Dia::Core::StringCRC& streamId = mod.writes[w];
                    if (!streamExists(streamId))
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' in PU '%s' writes unknown stream '%s'",
                            mod.instanceId.AsChar(), pu.instanceId.AsChar(), streamId.AsChar());
                        AddError("UNKNOWN_STREAM", msg.AsCStr(), mod.instanceId);
                    }
                }
            }

            // --- Cycle detection via Kahn's algorithm ---
            if (DetectCycle(pu))
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Circular dependency detected in processingUnit '%s'", pu.instanceId.AsChar());
                AddError("CYCLE_DETECTED", msg.AsCStr(), pu.instanceId);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckOrphanModules
    //
    // A non-"all" module is orphaned if none of its declared stages exist in
    // the manifest's stages array (i.e., the module would never be active).
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckOrphanModules(const ApplicationManifestV2& manifest)
    {
        // Build flat array of valid stage names
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> validStages;
        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
        {
            validStages.Add(manifest.stages[i].name);
        }

        const Dia::Core::StringCRC kAll("all");

        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const ModuleDeclaration& mod = pu.modules[m];

                // Check whether this module uses "all" sentinel
                bool isAllModule = false;
                for (unsigned int s = 0; s < mod.stages.Size(); ++s)
                {
                    if (mod.stages[s] == kAll)
                    {
                        isAllModule = true;
                        break;
                    }
                }

                // Skip "all" modules — they are always active
                if (isAllModule)
                {
                    continue;
                }

                // For non-"all" modules, at least one stage must be in manifest.stages
                bool hasValidStage = false;
                for (unsigned int s = 0; s < mod.stages.Size(); ++s)
                {
                    for (unsigned int vs = 0; vs < validStages.Size(); ++vs)
                    {
                        if (mod.stages[s] == validStages[vs])
                        {
                            hasValidStage = true;
                            break;
                        }
                    }
                    if (hasValidStage)
                    {
                        break;
                    }
                }

                if (!hasValidStage)
                {
                    Dia::Core::Containers::String256 msg;
                    msg.Format("Module '%s' in PU '%s' has no stages matching declared stages (orphan module)",
                        mod.instanceId.AsChar(), pu.instanceId.AsChar());
                    AddError("ORPHAN_MODULE", msg.AsCStr(), mod.instanceId);
                }
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckOrphanStreams
    //
    // Warns when a stream has no writers (nothing in writes[]) or no readers
    // (nothing in reads[]) across all PU modules.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckOrphanStreams(const ApplicationManifestV2& manifest)
    {
        for (unsigned int si = 0; si < manifest.streams.Size(); ++si)
        {
            const Dia::Core::StringCRC& streamId = manifest.streams[si].id;

            bool hasWriter = false;
            bool hasReader = false;

            for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
            {
                const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
                for (unsigned int m = 0; m < pu.modules.Size(); ++m)
                {
                    const ModuleDeclaration& mod = pu.modules[m];

                    for (unsigned int w = 0; w < mod.writes.Size(); ++w)
                    {
                        if (mod.writes[w] == streamId)
                        {
                            hasWriter = true;
                        }
                    }

                    for (unsigned int r = 0; r < mod.reads.Size(); ++r)
                    {
                        if (mod.reads[r] == streamId)
                        {
                            hasReader = true;
                        }
                    }
                }
            }

            if (!hasWriter)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' has no writers", streamId.AsChar());
                AddWarning("ORPHAN_STREAM", msg.AsCStr(), streamId);
            }

            if (!hasReader)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' has no readers", streamId.AsChar());
                AddWarning("ORPHAN_STREAM", msg.AsCStr(), streamId);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckEmptyStages
    //
    // Warns when a stage has only "all" modules and no stage-specific modules.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckEmptyStages(const ApplicationManifestV2& manifest)
    {
        const Dia::Core::StringCRC kAll("all");

        for (unsigned int si = 0; si < manifest.stages.Size(); ++si)
        {
            const Dia::Core::StringCRC& stageName = manifest.stages[si].name;

            bool hasStageSpecificModule = false;

            for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
            {
                const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
                for (unsigned int m = 0; m < pu.modules.Size(); ++m)
                {
                    const ModuleDeclaration& mod = pu.modules[m];

                    for (unsigned int s = 0; s < mod.stages.Size(); ++s)
                    {
                        if (mod.stages[s] == stageName)
                        {
                            // This module is specifically assigned to this stage (not "all")
                            hasStageSpecificModule = true;
                            break;
                        }
                    }

                    if (hasStageSpecificModule)
                    {
                        break;
                    }
                }

                if (hasStageSpecificModule)
                {
                    break;
                }
            }

            if (!hasStageSpecificModule)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stage '%s' has no stage-specific modules (only 'all' modules or none)",
                    stageName.AsChar());
                AddWarning("EMPTY_STAGE", msg.AsCStr(), stageName);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckMultiWriterViolations
    //
    // When a stream has multiWriter=false, at most one module across all PUs
    // may write to it.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckMultiWriterViolations(const ApplicationManifestV2& manifest)
    {
        for (unsigned int si = 0; si < manifest.streams.Size(); ++si)
        {
            const StreamDeclaration& stream = manifest.streams[si];

            if (stream.multiWriter)
            {
                continue;
            }

            unsigned int writerCount = 0;

            for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
            {
                const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
                for (unsigned int m = 0; m < pu.modules.Size(); ++m)
                {
                    const ModuleDeclaration& mod = pu.modules[m];
                    for (unsigned int w = 0; w < mod.writes.Size(); ++w)
                    {
                        if (mod.writes[w] == stream.id)
                        {
                            ++writerCount;
                        }
                    }
                }
            }

            if (writerCount > 1)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' has %u writers but multi_writer is false",
                    stream.id.AsChar(), writerCount);
                AddError("MULTI_WRITER_VIOLATION", msg.AsCStr(), stream.id);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // DetectCycle
    //
    // Kahn's algorithm (topological sort) — returns true if a cycle is detected.
    //
    // Steps:
    //   1. Build in-degree map: for each module, count how many other modules
    //      in the same PU list it as a dependency.
    //   2. Enqueue all modules with in-degree 0.
    //   3. Process the queue: dequeue a module, decrement in-degree of all
    //      modules that depend on it; enqueue those that reach in-degree 0.
    //   4. If processed count < total module count, a cycle exists.
    //
    // Note: "depends on X" means X must run before this module, so the edge
    // direction for topological sort is: dependency → dependent.
    //-----------------------------------------------------------------------------
    bool ManifestValidatorV2::DetectCycle(const ProcessingUnitDeclaration& pu)
    {
        const unsigned int moduleCount = pu.modules.Size();
        if (moduleCount == 0)
        {
            return false;
        }

        // In-degree array indexed parallel to pu.modules
        Dia::Core::Containers::DynamicArrayC<unsigned int, 32> inDegree;
        for (unsigned int i = 0; i < moduleCount; ++i)
        {
            inDegree.Add(0u);
        }

        // For each module, count how many of its declared dependencies exist
        // in this PU — those represent incoming edges to this module in the
        // dependency graph (dependency → this module).
        // In-degree of module[i] = number of modules that module[i] depends on
        // (i.e., how many predecessors it has).
        for (unsigned int i = 0; i < moduleCount; ++i)
        {
            const ModuleDeclaration& mod = pu.modules[i];
            for (unsigned int d = 0; d < mod.dependencies.Size(); ++d)
            {
                const Dia::Core::StringCRC& depId = mod.dependencies[d];
                // Verify the dependency exists in this PU (invalid deps are
                // reported by CheckPUModules; skip them here to avoid false cycles)
                for (unsigned int j = 0; j < moduleCount; ++j)
                {
                    if (pu.modules[j].instanceId == depId)
                    {
                        inDegree[i] = inDegree[i] + 1;
                        break;
                    }
                }
            }
        }

        // Queue (implemented as a DynamicArrayC acting as a simple FIFO via
        // a read-head index — avoids RemoveAt shifts)
        Dia::Core::Containers::DynamicArrayC<unsigned int, 32> queue;
        unsigned int queueHead = 0;

        for (unsigned int i = 0; i < moduleCount; ++i)
        {
            if (inDegree[i] == 0)
            {
                queue.Add(i);
            }
        }

        unsigned int processedCount = 0;

        while (queueHead < queue.Size())
        {
            const unsigned int current = queue[queueHead];
            ++queueHead;
            ++processedCount;

            const Dia::Core::StringCRC& currentId = pu.modules[current].instanceId;

            // Decrement in-degree of all modules that list currentId as a dependency
            for (unsigned int i = 0; i < moduleCount; ++i)
            {
                if (i == current)
                {
                    continue;
                }
                const ModuleDeclaration& mod = pu.modules[i];
                for (unsigned int d = 0; d < mod.dependencies.Size(); ++d)
                {
                    if (mod.dependencies[d] == currentId)
                    {
                        inDegree[i] = inDegree[i] - 1;
                        if (inDegree[i] == 0)
                        {
                            queue.Add(i);
                        }
                        // Each dependency entry is unique per module; once found, stop
                        break;
                    }
                }
            }
        }

        return processedCount < moduleCount;
    }

    //-----------------------------------------------------------------------------
    // IsReservedStreamId
    //
    // Reserved streams start with '$' (e.g. $lifecycle).  They are allowed in
    // module reads/writes without a corresponding manifest declaration.
    //-----------------------------------------------------------------------------
    /*static*/ bool ManifestValidatorV2::IsReservedStreamId(const Dia::Core::StringCRC& id)
    {
        const char* str = id.AsChar();
        return (str != nullptr && str[0] == '$');
    }

    //-----------------------------------------------------------------------------
    // CheckStreamReadsWritesBinding
    //
    // For each module in each PU, every entry in reads[] and writes[] must
    // reference a stream declared in manifest.streams — unless the stream ID
    // starts with '$', which marks it as a reserved/built-in stream.
    //
    // Error codes: UNKNOWN_STREAM_IN_READS, UNKNOWN_STREAM_IN_WRITES
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckStreamReadsWritesBinding(const ApplicationManifestV2& manifest)
    {
        // Build flat set of declared stream ids
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> declaredIds;
        for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
        {
            declaredIds.Add(manifest.streams[i].id);
        }

        auto isDeclared = [&declaredIds](const Dia::Core::StringCRC& id) -> bool {
            for (unsigned int i = 0; i < declaredIds.Size(); ++i)
            {
                if (declaredIds[i] == id)
                {
                    return true;
                }
            }
            return false;
        };

        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const ModuleDeclaration& mod = pu.modules[m];

                for (unsigned int r = 0; r < mod.reads.Size(); ++r)
                {
                    const Dia::Core::StringCRC& streamId = mod.reads[r];
                    if (IsReservedStreamId(streamId))
                    {
                        continue;  // reserved streams are always allowed in reads
                    }
                    if (!isDeclared(streamId))
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' in PU '%s' reads undeclared stream '%s'",
                            mod.instanceId.AsChar(), pu.instanceId.AsChar(), streamId.AsChar());
                        AddError("UNKNOWN_STREAM_IN_READS", msg.AsCStr(), mod.instanceId);
                    }
                }

                for (unsigned int w = 0; w < mod.writes.Size(); ++w)
                {
                    const Dia::Core::StringCRC& streamId = mod.writes[w];
                    if (IsReservedStreamId(streamId))
                    {
                        continue;
                    }
                    if (!isDeclared(streamId))
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' in PU '%s' writes undeclared stream '%s'",
                            mod.instanceId.AsChar(), pu.instanceId.AsChar(), streamId.AsChar());
                        AddError("UNKNOWN_STREAM_IN_WRITES", msg.AsCStr(), mod.instanceId);
                    }
                }
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckStreamPayloadTypes
    //
    // For each stream declaration: if kind is set but payload_type is empty
    // (Value() == 0), emit a PAYLOAD_TYPE_MISSING warning.  The actual type-tag
    // check happens at Connect() time in the handles — this is a manifest-level
    // early warning only.  No StreamTypeRegistry dependency.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckStreamPayloadTypes(const ApplicationManifestV2& manifest)
    {
        for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
        {
            const StreamDeclaration& decl = manifest.streams[i];

            if (decl.kind.Value() != 0 && decl.payloadType.Value() == 0)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' has kind '%s' but no payload_type — type will not be validated until Connect()",
                    decl.id.AsChar(), decl.kind.AsChar());
                AddWarning("PAYLOAD_TYPE_MISSING", msg.AsCStr(), decl.id);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckStreamOrphanReadersWriters
    //
    // For each declared stream, scan all module reads/writes across all PUs.
    // - Stream has readers but no writers → ORPHAN_READER_STREAM (warning)
    // - Stream has writers but no readers → ORPHAN_WRITER_STREAM (warning)
    //   (multiWriter streams still warn — write-only may be intentional logging
    //    but worth flagging)
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckStreamOrphanReadersWriters(const ApplicationManifestV2& manifest)
    {
        for (unsigned int si = 0; si < manifest.streams.Size(); ++si)
        {
            const StreamDeclaration& stream = manifest.streams[si];
            const Dia::Core::StringCRC& streamId = stream.id;

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
                        if (mod.reads[r] == streamId)
                        {
                            hasReader = true;
                        }
                    }

                    for (unsigned int w = 0; w < mod.writes.Size(); ++w)
                    {
                        if (mod.writes[w] == streamId)
                        {
                            hasWriter = true;
                        }
                    }
                }
            }

            if (hasReader && !hasWriter)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' has readers but no writers", streamId.AsChar());
                AddWarning("ORPHAN_READER_STREAM", msg.AsCStr(), streamId);
            }

            if (hasWriter && !hasReader)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' has writers but no readers", streamId.AsChar());
                AddWarning("ORPHAN_WRITER_STREAM", msg.AsCStr(), streamId);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckReservedPrefixViolations
    //
    // User-declared streams and module instance IDs must NOT start with '$'.
    // The '$' prefix is reserved for framework-auto-created streams (SD-018).
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckReservedPrefixViolations(const ApplicationManifestV2& manifest)
    {
        for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
        {
            const Dia::Core::StringCRC& id = manifest.streams[i].id;
            if (IsReservedStreamId(id))
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stream '%s' uses reserved '$' prefix — framework-owned streams cannot be declared in the manifest",
                    id.AsChar());
                AddError("RESERVED_PREFIX", msg.AsCStr(), id);
            }
        }

        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const Dia::Core::StringCRC& id = pu.modules[m].instanceId;
                if (IsReservedStreamId(id))
                {
                    Dia::Core::Containers::String256 msg;
                    msg.Format("Module instance_id '%s' in PU '%s' uses reserved '$' prefix",
                        id.AsChar(), pu.instanceId.AsChar());
                    AddError("RESERVED_PREFIX", msg.AsCStr(), id);
                }
            }
        }
    }

}} // namespace Dia::ApplicationFlow
