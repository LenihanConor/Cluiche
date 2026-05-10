#pragma once
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
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

        void Validate(const ApplicationManifestV2& manifest);

        bool HasErrors() const;
        bool HasWarnings() const;
        const Dia::Core::Containers::DynamicArrayC<ValidationEntry, 64>& GetResults() const;
        void Clear();

    private:
        const TypeRegistry& mRegistry;
        Dia::Core::Containers::DynamicArrayC<ValidationEntry, 64> mResults;

        void AddError(const char* code, const char* message, const Dia::Core::StringCRC& entityId);
        void AddWarning(const char* code, const char* message, const Dia::Core::StringCRC& entityId);

        void CheckStageReferences(const ApplicationManifestV2& manifest);
        void CheckDuplicatePUIds(const ApplicationManifestV2& manifest);
        void CheckDuplicateStreamIds(const ApplicationManifestV2& manifest);
        void CheckStreamPUReferences(const ApplicationManifestV2& manifest);
        void CheckPUModules(const ApplicationManifestV2& manifest);  // per-PU: duplicates, types, deps, streams, cycles
        void CheckOrphanModules(const ApplicationManifestV2& manifest);
        void CheckOrphanStreams(const ApplicationManifestV2& manifest);
        void CheckEmptyStages(const ApplicationManifestV2& manifest);
        void CheckMultiWriterViolations(const ApplicationManifestV2& manifest);

        // Kahn's algorithm cycle detection — returns true if cycle found
        bool DetectCycle(const ProcessingUnitDeclaration& pu);
    };

}} // namespace Dia::ApplicationFlow
