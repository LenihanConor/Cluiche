#pragma once
#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    enum class ValidationSeverity { Error, Warning };

    enum class ValidationRuleId
    {
        DependencyCycle,
        OrphanModule,
        UnknownStreamInReads,
        UnknownStreamInWrites,
        OrphanReaderStream,
        OrphanWriterStream,
        PayloadTypeMissing,
        StageRefInvalid,
        DuplicateInstanceId,
        InitialStageInvalid,
        StreamPUInvalid,
        StreamSelfLoop
    };

    struct ValidationIssue
    {
        ValidationRuleId ruleId;
        ValidationSeverity severity;
        char message[256];
    };

    struct ValidationResult
    {
        static constexpr unsigned int kMaxIssues = 64;
        Dia::Core::Containers::DynamicArrayC<ValidationIssue, kMaxIssues> issues;

        bool HasErrors() const;
        bool HasWarnings() const;
        unsigned int ErrorCount() const;
        unsigned int WarningCount() const;
    };

    class ManifestValidator
    {
    public:
        static ValidationResult Validate(const ManifestEditorState& state);
    };

}}} // namespace Dia::ApplicationFlow::Editor
