#pragma once
#ifndef DIA_SENSOR_AWARENESSBOARD_H
#define DIA_SENSOR_AWARENESSBOARD_H

#include <DiaMaths/Vector/Vector2D.h>
#include <cstdint>

namespace Dia::Sensor {

// Alert level driven by sensor perception — written by DefaultSensorBlackboardAdapter.
enum class AlertLevel : uint8_t {
    kIdle   = 0,
    kAlert,
    kCombat
};

// AwarenessBoard — AI concept struct written to the entity blackboard by
// DefaultSensorBlackboardAdapter.  Pure data; no DiaBlackboard dependency (AC-12).
struct AwarenessBoard {
    int                  knownEnemyCount         = 0;
    Dia::Maths::Vector2D lastKnownEnemyPosition;   // (0,0) until a sight result populates it
    AlertLevel           alertLevel              = AlertLevel::kIdle;
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_AWARENESSBOARD_H
