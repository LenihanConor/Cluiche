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

    void ManifestValidatorV2::Validate(const ApplicationManifestV3& manifest)
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
        CheckServiceStreamConstraints(manifest);
        CheckTransitionTargets(manifest);
        CheckAutoAdvanceConsistency(manifest);
        CheckTransitionSelfLoops(manifest);
        CheckStageReachability(manifest);
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
    void ManifestValidatorV2::CheckStageReferences(const ApplicationManifestV3& manifest)
    {
        // Build a flat array of valid stage names from manifest.stages
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> validStages;
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
    void ManifestValidatorV2::CheckDuplicatePUIds(const ApplicationManifestV3& manifest)
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
    void ManifestValidatorV2::CheckDuplicateStreamIds(const ApplicationManifestV3& manifest)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> seenIds;

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
    void ManifestValidatorV2::CheckStreamPUReferences(const ApplicationManifestV3& manifest)
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
    void ManifestValidatorV2::CheckPUModules(const ApplicationManifestV3& manifest)
    {
        // Build flat array of valid stream ids
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> streamIds;
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

                // Check channel stream references
                for (unsigned int c = 0; c < mod.channels.Size(); ++c)
                {
                    const Dia::Core::StringCRC& streamId = mod.channels[c].id;
                    if (IsReservedStreamId(streamId))
                        continue;  // reserved streams ($lifecycle, etc.) are always allowed
                    if (!streamExists(streamId))
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' in PU '%s' references unknown stream '%s' in channels",
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
    void ManifestValidatorV2::CheckOrphanModules(const ApplicationManifestV3& manifest)
    {
        // Build flat array of valid stage names
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> validStages;
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
    // Warns when a stream has no writers/providers or no readers/consumers
    // across all PU modules.
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckOrphanStreams(const ApplicationManifestV3& manifest)
    {
        static const Dia::Core::StringCRC kWrites("writes");
        static const Dia::Core::StringCRC kProvides("provides");
        static const Dia::Core::StringCRC kReads("reads");
        static const Dia::Core::StringCRC kConsumes("consumes");

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

                    for (unsigned int c = 0; c < mod.channels.Size(); ++c)
                    {
                        if (mod.channels[c].id == streamId)
                        {
                            if (mod.channels[c].role == kWrites || mod.channels[c].role == kProvides)
                                hasWriter = true;
                            if (mod.channels[c].role == kReads || mod.channels[c].role == kConsumes)
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
    void ManifestValidatorV2::CheckEmptyStages(const ApplicationManifestV3& manifest)
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
    // may write to it.  ServiceStream kind is skipped (providers handled by
    // CheckServiceStreamConstraints).
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckMultiWriterViolations(const ApplicationManifestV3& manifest)
    {
        static const Dia::Core::StringCRC kWrites("writes");
        static const Dia::Core::StringCRC kServiceKind("ServiceStream");

        for (unsigned int si = 0; si < manifest.streams.Size(); ++si)
        {
            const StreamDeclaration& stream = manifest.streams[si];

            if (stream.multiWriter)
            {
                continue;
            }

            // Skip ServiceStream — provider uniqueness is checked separately
            if (stream.kind == kServiceKind)
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
                    for (unsigned int c = 0; c < mod.channels.Size(); ++c)
                    {
                        if (mod.channels[c].id == stream.id && mod.channels[c].role == kWrites)
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
    // module channels without a corresponding manifest declaration.
    //-----------------------------------------------------------------------------
    /*static*/ bool ManifestValidatorV2::IsReservedStreamId(const Dia::Core::StringCRC& id)
    {
        const char* str = id.AsChar();
        return (str != nullptr && str[0] == '$');
    }

    //-----------------------------------------------------------------------------
    // CheckStreamReadsWritesBinding
    //
    // For each module in each PU, every channel entry must reference a stream
    // declared in manifest.streams — unless the stream ID starts with '$',
    // which marks it as a reserved/built-in stream.
    //
    // Additionally validates role/kind pairing:
    //   - reads/writes roles must NOT be used on ServiceStream kind
    //   - provides/consumes roles must ONLY be used on ServiceStream kind
    //
    // Error codes: UNKNOWN_STREAM_IN_READS, UNKNOWN_STREAM_IN_WRITES,
    //              SERVICE_STREAM_WRONG_ROLE, SERVICE_STREAM_ORPHAN_CONSUMER
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckStreamReadsWritesBinding(const ApplicationManifestV3& manifest)
    {
        static const Dia::Core::StringCRC kReads("reads");
        static const Dia::Core::StringCRC kWrites("writes");
        static const Dia::Core::StringCRC kProvides("provides");
        static const Dia::Core::StringCRC kConsumes("consumes");
        static const Dia::Core::StringCRC kServiceKind("ServiceStream");

        // Build flat set of declared stream ids
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> declaredIds;
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

        auto findStreamKind = [&manifest](const Dia::Core::StringCRC& id) -> Dia::Core::StringCRC {
            for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
            {
                if (manifest.streams[i].id == id)
                    return manifest.streams[i].kind;
            }
            return Dia::Core::StringCRC();
        };

        for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
        {
            const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
            for (unsigned int m = 0; m < pu.modules.Size(); ++m)
            {
                const ModuleDeclaration& mod = pu.modules[m];

                for (unsigned int c = 0; c < mod.channels.Size(); ++c)
                {
                    const ChannelBinding& ch = mod.channels[c];
                    if (IsReservedStreamId(ch.id))
                    {
                        continue;  // reserved streams are always allowed
                    }
                    if (!isDeclared(ch.id))
                    {
                        if (ch.role == kReads)
                        {
                            Dia::Core::Containers::String256 msg;
                            msg.Format("Module '%s' in PU '%s' reads undeclared stream '%s'",
                                mod.instanceId.AsChar(), pu.instanceId.AsChar(), ch.id.AsChar());
                            AddError("UNKNOWN_STREAM_IN_READS", msg.AsCStr(), mod.instanceId);
                        }
                        else if (ch.role == kWrites)
                        {
                            Dia::Core::Containers::String256 msg;
                            msg.Format("Module '%s' in PU '%s' writes undeclared stream '%s'",
                                mod.instanceId.AsChar(), pu.instanceId.AsChar(), ch.id.AsChar());
                            AddError("UNKNOWN_STREAM_IN_WRITES", msg.AsCStr(), mod.instanceId);
                        }
                        else
                        {
                            Dia::Core::Containers::String256 msg;
                            msg.Format("Module '%s' in PU '%s' references undeclared stream '%s' (role '%s')",
                                mod.instanceId.AsChar(), pu.instanceId.AsChar(), ch.id.AsChar(), ch.role.AsChar());
                            AddError("UNKNOWN_STREAM_IN_READS", msg.AsCStr(), mod.instanceId);
                        }
                        continue;
                    }

                    // Validate role/kind pairing
                    Dia::Core::StringCRC streamKind = findStreamKind(ch.id);
                    bool isService = (streamKind == kServiceKind);

                    if ((ch.role == kReads || ch.role == kWrites) && isService)
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' uses role '%s' on ServiceStream '%s' — use provides/consumes instead",
                            mod.instanceId.AsChar(), ch.role.AsChar(), ch.id.AsChar());
                        AddError("SERVICE_STREAM_WRONG_ROLE", msg.AsCStr(), mod.instanceId);
                    }

                    if ((ch.role == kProvides || ch.role == kConsumes) && !isService)
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("Module '%s' uses role '%s' on stream '%s' which is not a ServiceStream",
                            mod.instanceId.AsChar(), ch.role.AsChar(), ch.id.AsChar());
                        AddError("SERVICE_STREAM_ORPHAN_CONSUMER", msg.AsCStr(), mod.instanceId);
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
    void ManifestValidatorV2::CheckStreamPayloadTypes(const ApplicationManifestV3& manifest)
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
    // For each declared stream, scan all module channels across all PUs.
    // - role="reads"/"consumes" counts as a reader
    // - role="writes"/"provides" counts as a writer
    // - Stream has readers but no writers → ORPHAN_READER_STREAM (warning)
    // - Stream has writers but no readers → ORPHAN_WRITER_STREAM (warning)
    //   (multiWriter streams still warn — write-only may be intentional logging
    //    but worth flagging)
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckStreamOrphanReadersWriters(const ApplicationManifestV3& manifest)
    {
        static const Dia::Core::StringCRC kReads("reads");
        static const Dia::Core::StringCRC kWrites("writes");
        static const Dia::Core::StringCRC kProvides("provides");
        static const Dia::Core::StringCRC kConsumes("consumes");

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

                    for (unsigned int c = 0; c < mod.channels.Size(); ++c)
                    {
                        if (mod.channels[c].id == streamId)
                        {
                            if (mod.channels[c].role == kReads || mod.channels[c].role == kConsumes)
                                hasReader = true;
                            if (mod.channels[c].role == kWrites || mod.channels[c].role == kProvides)
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
    // CheckServiceStreamConstraints
    //
    // ServiceStream-specific validation:
    //   - Exactly one provider per ServiceStream
    //   - At least one consumer per ServiceStream (orphan provider warning)
    //   - Consumer PU must not appear before provider PU in config ordering
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckServiceStreamConstraints(const ApplicationManifestV3& manifest)
    {
        static const Dia::Core::StringCRC kServiceKind("ServiceStream");
        static const Dia::Core::StringCRC kProvides("provides");
        static const Dia::Core::StringCRC kConsumes("consumes");

        for (unsigned int si = 0; si < manifest.streams.Size(); ++si)
        {
            const StreamDeclaration& stream = manifest.streams[si];
            if (stream.kind != kServiceKind)
                continue;

            // Pass 1: collect provider and consumer PU indices
            unsigned int providerCount    = 0;
            unsigned int consumerCount    = 0;
            Dia::Core::StringCRC providerPU;
            unsigned int providerPUIndex  = 0xFFFFFFFF;

            // Collect per-consumer PU info for ordering check (pass 2)
            Dia::Core::Containers::DynamicArrayC<unsigned int, 8> consumerPUIndices;
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> consumerPUIds;

            for (unsigned int p = 0; p < manifest.processingUnits.Size(); ++p)
            {
                const ProcessingUnitDeclaration& pu = manifest.processingUnits[p];
                for (unsigned int m = 0; m < pu.modules.Size(); ++m)
                {
                    const ModuleDeclaration& mod = pu.modules[m];
                    for (unsigned int c = 0; c < mod.channels.Size(); ++c)
                    {
                        if (mod.channels[c].id != stream.id)
                            continue;
                        if (mod.channels[c].role == kProvides)
                        {
                            ++providerCount;
                            providerPU      = pu.instanceId;
                            providerPUIndex = p;
                        }
                        else if (mod.channels[c].role == kConsumes)
                        {
                            ++consumerCount;
                            if (!consumerPUIndices.IsFull())
                            {
                                consumerPUIndices.Add(p);
                                consumerPUIds.Add(pu.instanceId);
                            }
                        }
                    }
                }
            }

            // Pass 2: ordering check (requires both provider and consumer to be known)
            if (providerPUIndex != 0xFFFFFFFF)
            {
                for (unsigned int ci = 0; ci < consumerPUIndices.Size(); ++ci)
                {
                    if (consumerPUIndices[ci] < providerPUIndex)
                    {
                        Dia::Core::Containers::String256 msg;
                        msg.Format("ServiceStream '%s': consumer PU '%s' (index %u) appears before provider PU '%s' (index %u) in config",
                            stream.id.AsChar(), consumerPUIds[ci].AsChar(), consumerPUIndices[ci],
                            providerPU.AsChar(), providerPUIndex);
                        AddError("SERVICE_STREAM_PROVIDER_AFTER_CONSUMER", msg.AsCStr(), stream.id);
                    }
                }
            }

            // kServiceStreamMissingProvider
            if (providerCount == 0)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("ServiceStream '%s' has no provider (no module has role=provides)", stream.id.AsChar());
                AddError("SERVICE_STREAM_MISSING_PROVIDER", msg.AsCStr(), stream.id);
            }

            // kServiceStreamMultipleProviders
            if (providerCount > 1)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("ServiceStream '%s' has %u providers — exactly one provider required", stream.id.AsChar(), providerCount);
                AddError("SERVICE_STREAM_MULTIPLE_PROVIDERS", msg.AsCStr(), stream.id);
            }

            // kServiceStreamOrphanProvider
            if (providerCount == 1 && consumerCount == 0)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("ServiceStream '%s' has a provider but no consumers", stream.id.AsChar());
                AddError("SERVICE_STREAM_ORPHAN_PROVIDER", msg.AsCStr(), stream.id);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckReservedPrefixViolations
    //
    // User-declared streams and module instance IDs must NOT start with '$'.
    // The '$' prefix is reserved for framework-auto-created streams (SD-018).
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckReservedPrefixViolations(const ApplicationManifestV3& manifest)
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

    //-----------------------------------------------------------------------------
    // CheckTransitionTargets
    //
    // Every entry in a stage's transitions[] must name a stage declared in
    // manifest.stages.
    // Error: TRANSITION_TARGET_INVALID
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckTransitionTargets(const ApplicationManifestV3& manifest)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> validStages;
        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
            validStages.Add(manifest.stages[i].name);

        auto stageExists = [&validStages](const Dia::Core::StringCRC& name) -> bool {
            for (unsigned int i = 0; i < validStages.Size(); ++i)
                if (validStages[i] == name) return true;
            return false;
        };

        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
        {
            const StageDeclaration& stage = manifest.stages[i];
            for (unsigned int t = 0; t < stage.transitions.Size(); ++t)
            {
                if (!stageExists(stage.transitions[t]))
                {
                    Dia::Core::Containers::String256 msg;
                    msg.Format("Stage '%s' transition target '%s' is not a declared stage",
                        stage.name.AsChar(), stage.transitions[t].AsChar());
                    AddError("TRANSITION_TARGET_INVALID", msg.AsCStr(), stage.name);
                }
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckAutoAdvanceConsistency
    //
    // auto_advance=true is only valid when transitions.length == 1.
    // Error: AUTO_ADVANCE_AMBIGUOUS
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckAutoAdvanceConsistency(const ApplicationManifestV3& manifest)
    {
        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
        {
            const StageDeclaration& stage = manifest.stages[i];
            if (stage.autoAdvance && stage.transitions.Size() != 1)
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stage '%s' has auto_advance=true but transitions.length=%u (must be exactly 1)",
                    stage.name.AsChar(), stage.transitions.Size());
                AddError("AUTO_ADVANCE_AMBIGUOUS", msg.AsCStr(), stage.name);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckTransitionSelfLoops
    //
    // A stage whose transitions[] contains its own name is a self-loop.
    // Legitimate (re-enter stage = hot-reload via SD-012) but worth flagging.
    // Warning: TRANSITION_SELF_LOOP
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckTransitionSelfLoops(const ApplicationManifestV3& manifest)
    {
        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
        {
            const StageDeclaration& stage = manifest.stages[i];
            for (unsigned int t = 0; t < stage.transitions.Size(); ++t)
            {
                if (stage.transitions[t] == stage.name)
                {
                    Dia::Core::Containers::String256 msg;
                    msg.Format("Stage '%s' has a self-loop in its transitions[]",
                        stage.name.AsChar());
                    AddWarning("TRANSITION_SELF_LOOP", msg.AsCStr(), stage.name);
                    break; // one warning per stage is enough
                }
            }
        }
    }

    //-----------------------------------------------------------------------------
    // CheckStageReachability
    //
    // A non-initial stage that no other stage lists in its transitions[] can
    // never be entered.  Warning: STAGE_UNREACHABLE
    //-----------------------------------------------------------------------------
    void ManifestValidatorV2::CheckStageReachability(const ApplicationManifestV3& manifest)
    {
        // Build the union set of all stages that appear as a transition target
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> referenced;
        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
        {
            const StageDeclaration& stage = manifest.stages[i];
            for (unsigned int t = 0; t < stage.transitions.Size(); ++t)
            {
                const Dia::Core::StringCRC& target = stage.transitions[t];
                bool alreadyIn = false;
                for (unsigned int r = 0; r < referenced.Size(); ++r)
                    if (referenced[r] == target) { alreadyIn = true; break; }
                if (!alreadyIn)
                    referenced.Add(target);
            }
        }

        auto isReferenced = [&referenced](const Dia::Core::StringCRC& name) -> bool {
            for (unsigned int i = 0; i < referenced.Size(); ++i)
                if (referenced[i] == name) return true;
            return false;
        };

        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
        {
            const StageDeclaration& stage = manifest.stages[i];
            if (stage.name == manifest.initialStage)
                continue; // initial stage is always reachable by definition
            if (!isReferenced(stage.name))
            {
                Dia::Core::Containers::String256 msg;
                msg.Format("Stage '%s' is not the initial stage and is not referenced by any transition",
                    stage.name.AsChar());
                AddWarning("STAGE_UNREACHABLE", msg.AsCStr(), stage.name);
            }
        }
    }

}} // namespace Dia::ApplicationFlow
