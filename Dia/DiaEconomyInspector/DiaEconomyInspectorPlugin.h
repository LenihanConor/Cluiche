#pragma once
#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>
#include <DiaEconomyInspector/Controllers/EconomyInstancesController.h>
#include <DiaEconomyInspector/Controllers/EconomyModifiersController.h>
#include <DiaEconomyInspector/Controllers/EconomyEventsController.h>
#include <DiaEconomyInspector/Controllers/EconomySchemaController.h>

namespace Dia::EconomyInspector {

class DiaEconomyInspectorPlugin final : public Dia::Editor::LiveConnectionPluginBase {
public:
    DiaEconomyInspectorPlugin();
protected:
    void OnLivePluginLoad()   override;
    void OnLivePluginUnload() override;
    void OnGameConnected()    override;
    void OnGameDisconnected() override;
private:
    EconomyInstancesController  mInstancesController;
    EconomyModifiersController  mModifiersController;
    EconomyEventsController     mEventsController;
    EconomySchemaController     mSchemaController;
};
} // namespace Dia::EconomyInspector
