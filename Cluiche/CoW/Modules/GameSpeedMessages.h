#pragma once
#include <Shared/GameSpeedController.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace CoW {

// Published by GameSpeedControllerModule once per game day. Every CoW gameplay system
// advances on this. Gate coarser cadences on `band` (e.g. band >= TimeBand::Season).
struct SimulationTick
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.SimulationTick" };
    uint64_t day  = 0;
    TimeBand band = TimeBand::Day;
};

// GDD's PauseRequested is SpeedChangeRequested{Paused}.
struct SpeedChangeRequested
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.SpeedChangeRequested" };
    SpeedTier tier = SpeedTier::Paused;
};

struct ManualAdvanceRequested
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.ManualAdvanceRequested" };
};

struct ModalOpened
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.ModalOpened" };
};

struct ModalClosed
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.ModalClosed" };
};

// Published by S-1 WLD when a new world is ready.
struct WorldGenerationComplete
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.WorldGenerationComplete" };
};

} // namespace CoW
