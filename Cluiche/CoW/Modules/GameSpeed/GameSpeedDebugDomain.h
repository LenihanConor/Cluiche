// Panel-only IDebugDomain for GameSpeedController: calendar, speed and pause state, with a
// speed slider (0 = Paused ... 4 = Fastest). Reads the controller through the module and
// requests changes by posting SpeedChangeRequested, so the panel never touches the state directly.
#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace CoW {

class GameSpeedControllerModule;

class GameSpeedDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    explicit GameSpeedDebugDomain(GameSpeedControllerModule& module);

    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return false; }

    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

private:
    GameSpeedControllerModule& mModule;
};

} // namespace CoW

#endif // DIA_DEBUG
