#include <DiaApplicationFlowEditor/V2/RiskAssessor.h>
#include <DiaApplicationFlowEditor/V2/Commands/PUCommands.h>
#include <DiaApplicationFlowEditor/V2/Commands/ModuleCommands.h>
#include <DiaApplicationFlowEditor/V2/Commands/StreamCommands.h>
#include <DiaApplicationFlowEditor/V2/Commands/StageCommands.h>
#include <cstdio>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    RiskReport RiskAssessor::Assess(const ICommand& command,
                                    const ManifestEditorState& /*state*/,
                                    bool isLiveConnected)
    {
        RiskReport report{};
        report.condition = RiskCondition::None;
        report.description[0] = '\0';

        if (!isLiveConnected)
            return report;

        if (dynamic_cast<const RemovePUCommand*>(&command))
        {
            report.condition = RiskCondition::RemovePU;
            snprintf(report.description, sizeof(report.description),
                     "Removing a processing unit while the game is running will crash the live instance.");
            return report;
        }

        if (dynamic_cast<const RemoveModuleCommand*>(&command))
        {
            report.condition = RiskCondition::RemoveModule;
            snprintf(report.description, sizeof(report.description),
                     "Removing a module while the game is running may cause undefined behaviour in the live instance.");
            return report;
        }

        if (dynamic_cast<const SetStreamCapacityCommand*>(&command))
        {
            report.condition = RiskCondition::ChangeStreamCapacity;
            snprintf(report.description, sizeof(report.description),
                     "Changing stream capacity while the game is running may corrupt in-flight messages.");
            return report;
        }

        if (dynamic_cast<const SetStreamMaxReadersCommand*>(&command))
        {
            report.condition = RiskCondition::ChangeStreamMaxReaders;
            snprintf(report.description, sizeof(report.description),
                     "Changing stream max-readers while the game is running may corrupt reader registration.");
            return report;
        }

        if (dynamic_cast<const SetPUFrequencyCommand*>(&command))
        {
            report.condition = RiskCondition::ChangeFrequency;
            snprintf(report.description, sizeof(report.description),
                     "Changing processing unit frequency while the game is running can cause missed frames.");
            return report;
        }

        if (dynamic_cast<const RemoveStageCommand*>(&command))
        {
            report.condition = RiskCondition::RemoveStage;
            snprintf(report.description, sizeof(report.description),
                     "Removing a stage while the game is running may invalidate the active stage reference.");
            return report;
        }

        return report;
    }

}}} // namespace Dia::ApplicationFlow::Editor
