#pragma once
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String256.h>

namespace Dia { namespace ApplicationFlow {

    enum class ValidationSeverity { kError, kWarning };

    struct ValidationEntry {
        ValidationSeverity severity;
        Dia::Core::StringCRC code;
        Dia::Core::Containers::String256 message;
        Dia::Core::StringCRC entityId;
    };

    class ManifestValidatorV2 {
    public:
        explicit ManifestValidatorV2(const TypeRegistry& registry);

        void Validate(const ApplicationManifestV3& manifest);

        bool HasErrors() const;
        bool HasWarnings() const;
        const Dia::Core::Containers::DynamicArrayC<ValidationEntry, 64>& GetResults() const;
        void Clear();

    private:
        const TypeRegistry& mRegistry;
        Dia::Core::Containers::DynamicArrayC<ValidationEntry, 64> mResults;

        void AddError(const char* code, const char* message, const Dia::Core::StringCRC& entityId);
        void AddWarning(const char* code, const char* message, const Dia::Core::StringCRC& entityId);

        void CheckStageReferences(const ApplicationManifestV3& manifest);
        void CheckDuplicatePUIds(const ApplicationManifestV3& manifest);
        void CheckDuplicateStreamIds(const ApplicationManifestV3& manifest);
        void CheckStreamPUReferences(const ApplicationManifestV3& manifest);
        void CheckPUModules(const ApplicationManifestV3& manifest);  // per-PU: duplicates, types, deps, streams, cycles
        void CheckOrphanModules(const ApplicationManifestV3& manifest);
        void CheckOrphanStreams(const ApplicationManifestV3& manifest);
        void CheckEmptyStages(const ApplicationManifestV3& manifest);
        void CheckMultiWriterViolations(const ApplicationManifestV3& manifest);

        void CheckStreamReadsWritesBinding(const ApplicationManifestV3& manifest);   // channels reference declared streams
        void CheckStreamPayloadTypes(const ApplicationManifestV3& manifest);          // payload_type registered
        void CheckStreamOrphanReadersWriters(const ApplicationManifestV3& manifest);  // orphan reader/writer detection
        void CheckReservedPrefixViolations(const ApplicationManifestV3& manifest);    // user $ prefix forbidden
        void CheckServiceStreamConstraints(const ApplicationManifestV3& manifest);    // ServiceStream provider/consumer rules

        // v3 transition rules
        void CheckTransitionTargets(const ApplicationManifestV3& manifest);           // TRANSITION_TARGET_INVALID (E)
        void CheckAutoAdvanceConsistency(const ApplicationManifestV3& manifest);      // AUTO_ADVANCE_AMBIGUOUS (E)
        void CheckTransitionSelfLoops(const ApplicationManifestV3& manifest);         // TRANSITION_SELF_LOOP (W)
        void CheckStageReachability(const ApplicationManifestV3& manifest);           // STAGE_UNREACHABLE (W)

        // Kahn's algorithm cycle detection — returns true if cycle found
        bool DetectCycle(const ProcessingUnitDeclaration& pu);

        // Returns true if the stream id is a reserved stream (e.g. $lifecycle)
        static bool IsReservedStreamId(const Dia::Core::StringCRC& id);
    };

}} // namespace Dia::ApplicationFlow
