#pragma once
#ifndef DIA_SENSOR_SOUNDTYPE_H
#define DIA_SENSOR_SOUNDTYPE_H

#include <cstdint>

namespace Dia::Sensor {

enum class SoundType : uint8_t {
    kFootstep = 0,
    kExplosion,
    kAbilityCast,
    kUnknown
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_SOUNDTYPE_H
