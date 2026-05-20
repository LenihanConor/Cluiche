#pragma once
#include <DiaApplicationEditor/V2/Commands/ICommand.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    enum class RiskCondition
    {
        None = 0,
        RemovePU,
        RemoveModule,
        ChangeStreamCapacity,
        ChangeStreamMaxReaders,
        ChangeFrequency,
        RemoveStage
    };

    struct RiskReport
    {
        RiskCondition condition;
        char description[256];

        bool HasRisk() const { return condition != RiskCondition::None; }
    };

    class RiskAssessor
    {
    public:
        static RiskReport Assess(const ICommand& command,
                                 const ManifestEditorState& state,
                                 bool isLiveConnected);
    };

}}} // namespace Dia::ApplicationFlow::Editor
