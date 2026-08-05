#pragma once
#ifndef DIA_SENSOR_SENSORRESULTS_H
#define DIA_SENSOR_SENSORRESULTS_H

#include <DiaEntity/Entity.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaSensor/SoundType.h>

namespace Dia::Sensor {

struct SightResult {
    Dia::Entity::Entity entity;
    float               distance  = 0.f;
    float               angle     = 0.f;   // radians from sensor forward dir
    int                 timestamp = 0;     // frame number
};

struct ProximityResult {
    Dia::Entity::Entity entity;
    float               distance = 0.f;
};

struct DamageEvent {
    Dia::Entity::Entity source;
    float               amount    = 0.f;
    int                 timestamp = 0;
};

struct SoundEvent {
    Dia::Maths::Vector2D position;
    SoundType            type      = SoundType::kUnknown;
    int                  timestamp = 0;
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_SENSORRESULTS_H
