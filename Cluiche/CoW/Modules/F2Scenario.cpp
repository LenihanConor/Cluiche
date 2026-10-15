#include "Modules/F2Scenario.h"
#include <cmath>
#include <cstdio>

namespace CoW {

namespace {

constexpr float kScaleTolerance = 1e-4f;

F2ScenarioAction A(uint32_t tick, F2ActionKind kind, SpeedTier tier = SpeedTier::Normal)
{
    F2ScenarioAction a;
    a.tick = tick;
    a.kind = kind;
    a.tier = tier;
    return a;
}

std::vector<F2ScenarioAction> BuildSession()
{
    using K = F2ActionKind;
    using T = SpeedTier;
    return {
        A(10,  K::SetSpeed, T::Slow),
        A(30,  K::SetSpeed, T::Normal),
        A(50,  K::SetSpeed, T::Fast),
        A(70,  K::SetSpeed, T::Fastest),
        A(90,  K::SetSpeed, T::Slow),
        A(110, K::SetSpeed, T::Fast),
        A(130, K::PlayerPause),
        A(150, K::ManualStep),                    // paused: exactly one day
        A(170, K::ManualStep),
        A(190, K::PlayerResume, T::Normal),
        A(210, K::SetSpeed, T::Fastest),
        A(230, K::OpenModal),                     // auto-pause, depth 1
        A(250, K::OpenModal),                     // depth 2
        A(270, K::CloseModal),                    // depth 1: must stay paused
        A(290, K::SetSpeed, T::Fast),             // speed request while a modal is open
        A(310, K::CloseModal),                    // depth 0: resumes at Fast
        A(330, K::PlayerPause),
        A(350, K::OpenModal),                     // modal over a player pause
        A(370, K::CloseModal),                    // player pause survives (EC-01)
        A(390, K::ManualStep),
        A(410, K::PlayerResume, T::Fastest),
        A(490, K::PlayerPause),                   // stopped and modal-free for the save at tick 500
    };
}

std::vector<F2ScenarioAction> BuildRemainder()
{
    using K = F2ActionKind;
    using T = SpeedTier;
    return {
        A(500, K::SetSpeed, T::Normal),           // explicit first action after the save
        A(530, K::SetSpeed, T::Fastest),
        A(600, K::SetSpeed, T::Fast),
        A(660, K::SetSpeed, T::Fastest),
        A(760, K::SetSpeed, T::Fast),
        A(820, K::SetSpeed, T::Fastest),
        A(900, K::SetSpeed, T::Normal),
        A(940, K::SetSpeed, T::Fastest),
    };
}

} // namespace

const char* F2ActionKindName(F2ActionKind kind)
{
    switch (kind)
    {
    case F2ActionKind::SetSpeed:     return "SetSpeed";
    case F2ActionKind::PlayerPause:  return "PlayerPause";
    case F2ActionKind::PlayerResume: return "PlayerResume";
    case F2ActionKind::OpenModal:    return "OpenModal";
    case F2ActionKind::CloseModal:   return "CloseModal";
    case F2ActionKind::ManualStep:   return "ManualStep";
    }
    return "Unknown";
}

const std::vector<F2ScenarioAction>& F2SessionScript()
{
    static const std::vector<F2ScenarioAction> script = BuildSession();
    return script;
}

const std::vector<F2ScenarioAction>& F2RemainderScript()
{
    static const std::vector<F2ScenarioAction> script = BuildRemainder();
    return script;
}

int F2DrawsForTick(uint32_t tick, uint8_t (&streamsOut)[kF2MaxDrawsPerTick])
{
    // 1..4 draws, uneven across ticks; streams chosen by a fixed function of the tick.
    const int count = 1 + static_cast<int>((tick % 3) + ((tick / 7) % 2));
    for (int i = 0; i < count; ++i)
        streamsOut[i] = static_cast<uint8_t>((tick * 5 + static_cast<uint32_t>(i) * 3 + tick / 11) % 8);
    return count;
}

void F2ApplyAction(F2ExpectedClock& m, const F2ScenarioAction& action)
{
    switch (action.kind)
    {
    case F2ActionKind::SetSpeed:
    case F2ActionKind::PlayerResume:
    case F2ActionKind::PlayerPause:
    {
        const SpeedTier tier = (action.kind == F2ActionKind::PlayerPause) ? SpeedTier::Paused : action.tier;
        if (m.modalDepth > 0)
        {
            // Stays paused while a modal is open; a running speed is remembered for the close.
            if (tier == SpeedTier::Paused)
                m.source = PauseSource::Player;
            else
            {
                m.preModal = tier;
                m.source = PauseSource::Modal;
            }
        }
        else if (tier == SpeedTier::Paused)
        {
            m.tier = SpeedTier::Paused;
            m.source = PauseSource::Player;
        }
        else
        {
            m.tier = tier;
            m.source = PauseSource::None;
        }
        break;
    }
    case F2ActionKind::OpenModal:
        if (m.modalDepth == 0 && m.tier != SpeedTier::Paused)
        {
            m.preModal = m.tier;
            m.tier = SpeedTier::Paused;
            m.source = PauseSource::Modal;
        }
        if (m.modalDepth < 255)
            ++m.modalDepth;
        break;
    case F2ActionKind::CloseModal:
        if (m.modalDepth > 0)
            --m.modalDepth;
        if (m.modalDepth == 0 && m.source == PauseSource::Modal)
        {
            m.tier = m.preModal;
            m.source = PauseSource::None;
        }
        break;
    case F2ActionKind::ManualStep:
        break;   // no state change; see F2ManualStepAccepted
    }
}

bool F2ManualStepAccepted(const F2ExpectedClock& model)
{
    return model.tier == SpeedTier::Paused;
}

float F2ExpectedTimeScale(SpeedTier tier, const SpeedPresetConfig& config)
{
    if (tier == SpeedTier::Paused || !IsValidSpeedTier(tier))
        return 0.0f;
    return 1.0f / config.wallSecondsPerDay[static_cast<int>(tier) - 1];
}

std::string F2CompareClock(const F2ExpectedClock& e, const F2ObservedClock& o, const SpeedPresetConfig& config)
{
    char buf[160];
    if (o.tier != e.tier)
    {
        std::snprintf(buf, sizeof(buf), "speed tier %s, expected %s", SpeedTierName(o.tier), SpeedTierName(e.tier));
        return buf;
    }
    if (o.source != e.source)
    {
        std::snprintf(buf, sizeof(buf), "pause source %d, expected %d", static_cast<int>(o.source), static_cast<int>(e.source));
        return buf;
    }
    if (o.modalDepth != e.modalDepth)
    {
        std::snprintf(buf, sizeof(buf), "modalDepth %u, expected %u", static_cast<unsigned>(o.modalDepth), static_cast<unsigned>(e.modalDepth));
        return buf;
    }
    const float expectedScale = F2ExpectedTimeScale(e.tier, config);
    if (std::fabs(o.controllerScale - expectedScale) > kScaleTolerance)
    {
        std::snprintf(buf, sizeof(buf), "controller time scale %.5f, expected %.5f (%s)", o.controllerScale, expectedScale, SpeedTierName(e.tier));
        return buf;
    }
    if (std::fabs(o.domainScale - expectedScale) > kScaleTolerance)
    {
        std::snprintf(buf, sizeof(buf), "world time scale %.5f, expected %.5f (%s)", o.domainScale, expectedScale, SpeedTierName(e.tier));
        return buf;
    }
    return std::string();
}

} // namespace CoW
