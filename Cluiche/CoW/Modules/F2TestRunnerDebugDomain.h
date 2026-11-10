// Panel-only IDebugDomain for the F-2 TestRunnerPanel. State out: the F2PanelModel as JSON (phases,
// checks, fingerprints, button state). Commands in: run_scenario, set_speed {tier}, step,
// modal_open, modal_close. The domain holds no scenario logic: it reads the runner and posts
// requests (RunScenario / the same bus messages the scenario script uses). Manual commands are
// refused while the scenario runs. The panel UI is the "f2_test_runner" card in
// Assets/CoW/Global/Presentation/UI/debug-panel.html.
#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace CoW {

class F2TestRunnerModule;

class F2TestRunnerDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    explicit F2TestRunnerDebugDomain(F2TestRunnerModule& runner);

    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return false; }

    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // Commands accepted and acted on / refused (unknown, malformed, or locked while running).
    unsigned GetAcceptedCommandCount() const { return mAccepted; }
    unsigned GetRefusedCommandCount()  const { return mRefused; }

private:
    F2TestRunnerModule& mRunner;
    unsigned mAccepted = 0;
    unsigned mRefused = 0;
};

} // namespace CoW

#endif // DIA_DEBUG
