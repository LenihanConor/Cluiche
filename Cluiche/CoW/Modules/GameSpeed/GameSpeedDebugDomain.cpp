#include "Modules/GameSpeedDebugDomain.h"
#ifdef DIA_DEBUG

#include "Modules/GameSpeedControllerModule.h"
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <cmath>

namespace CoW {

namespace {
    const Dia::Core::StringCRC kCmdSetScale("setScale");

    const char* PauseSourceName(PauseSource source)
    {
        switch (source)
        {
        case PauseSource::None:   return "none";
        case PauseSource::Player: return "player";
        case PauseSource::Modal:  return "modal";
        }
        return "unknown";
    }
}

GameSpeedDebugDomain::GameSpeedDebugDomain(GameSpeedControllerModule& module)
    : mModule(module)
{}

Dia::Core::StringCRC GameSpeedDebugDomain::GetDomainId()    const { return Dia::Core::StringCRC("GameSpeed"); }
const char*          GameSpeedDebugDomain::GetDisplayName() const { return "Game Speed"; }
const char*          GameSpeedDebugDomain::GetDescription() const { return "Game calendar, speed preset and pause state"; }
Dia::Core::StringCRC GameSpeedDebugDomain::GetGroup()       const { return Dia::Core::StringCRC("Systems"); }
Dia::Core::RGBA      GameSpeedDebugDomain::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kCoreDebug; }

void GameSpeedDebugDomain::GetJSONState(Json::Value& out)
{
    const GameSpeedController& controller = mModule.GetController();
    const GameSpeedState& state = controller.GetState();
    const uint64_t day = state.dayCounter;

    out["drawers"] = Json::Value(Json::arrayValue);

    Json::Value& stats = out["stats"] = Json::Value(Json::objectValue);
    stats["day"]          = static_cast<Json::UInt64>(day);
    stats["year"]         = static_cast<Json::UInt64>(YearOf(day));
    stats["season"]       = static_cast<Json::UInt64>(SeasonOf(day) % kSeasonsPerYear);
    stats["week"]         = static_cast<Json::UInt64>(WeekOf(day) % kWeeksPerSeason);
    stats["dayOfWeek"]    = static_cast<Json::UInt64>(day % kDaysPerWeek);
    stats["speed"]        = SpeedTierName(state.speedTier);
    stats["paused"]       = controller.IsPaused();
    stats["pauseSource"]  = PauseSourceName(state.pauseSource);
    stats["modalDepth"]   = static_cast<int>(state.modalDepth);
    stats["timeScale"]    = controller.GetTimeScale();
    stats["fallbackConfig"] = mModule.IsUsingFallbackConfig();

    Json::Value& controls = out["controls"] = Json::Value(Json::arrayValue);
    Json::Value speed(Json::objectValue);
    speed["key"]   = "speed";
    speed["label"] = "Speed (0=Pause 4=Fastest)";
    speed["min"]   = 0;
    speed["max"]   = 4;
    speed["step"]  = 1;
    speed["value"] = static_cast<int>(state.speedTier);
    controls.append(speed);
}

void GameSpeedDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd != kCmdSetScale)
        return;
    if (!args.isMember("key") || !args["key"].isString() || args["key"].asString() != "speed")
        return;
    if (!args.isMember("value") || !args["value"].isNumeric())
        return;

    const long tier = std::lround(args["value"].asDouble());
    if (tier < 0 || tier > static_cast<long>(SpeedTier::Fastest))
        return;
    mModule.RequestSpeed(static_cast<SpeedTier>(tier));
}

} // namespace CoW

#endif // DIA_DEBUG
