#pragma once
#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>
#include <DiaAIInspector/Controllers/AIBudgetController.h>
#include <DiaAIInspector/Controllers/UtilityAIController.h>
#include <DiaAIInspector/Controllers/RulesController.h>
#include <DiaAIInspector/Controllers/HTNController.h>

namespace Dia::AIInspector {

class DiaAIInspectorPlugin final : public Dia::Editor::LiveConnectionPluginBase {
public:
    DiaAIInspectorPlugin();

protected:
    void OnLivePluginLoad()   override;
    void OnLivePluginUnload() override;
    void OnGameConnected()    override;
    void OnGameDisconnected() override;

private:
    AIBudgetController   mBudgetController;
    UtilityAIController  mUtilityAIController;
    RulesController      mRulesController;
    HTNController        mHTNController;
};

} // namespace Dia::AIInspector
