#pragma once
// Hand-written POD (no cow_worldsim.diagamemessages or `dia codegen messages` output exists for CoW yet), following
// GameSpeedMessages.h. Bus messages must stay trivially copyable.
#include <DiaCore/CRC/StringCRC.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>
#include <cstdint>

namespace CoW {

// Published by the weather stage once per committed live step (never during spin-up). A notification only: subscribers
// read the committed snapshot in response.
struct WeatherUpdated
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.WeatherUpdated" };
    uint64_t week = 0;
    Draoi::HexWorld::Weather::Season season = Draoi::HexWorld::Weather::Season::Spring;
};

} // namespace CoW
