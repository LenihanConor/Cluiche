#pragma once
#include <DiaApplicationFlowEditor/V2/ManifestEditorState.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    enum class ValidationSeverity { Error, Warning };

    enum class ValidationTargetKind : unsigned char { None, PU, Module, Stream };

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

    struct SuggestedCommand
    {
        char commandType[32]  = {};
        char puId[64]         = {};
        char instanceId[64]   = {};
        char streamId[64]     = {};
        char role[16]         = {};
        char stagesCSV[256]   = {};
    };

    struct ValidationIssue
    {
        ValidationRuleId ruleId;
        ValidationSeverity severity;
        char message[256];

        ValidationTargetKind targetKind = ValidationTargetKind::None;
        char targetPuId[64]     = {};
        char targetModuleId[64] = {};
        char targetStreamId[64] = {};

        char suggestedActionLabel[64] = {};
        SuggestedCommand suggestedCommand = {};
    };

    struct ValidationResult
    {
        static constexpr unsigned int kMaxIssues = 64;
        Dia::Core::Containers::DynamicArray<ValidationIssue> issues;

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
