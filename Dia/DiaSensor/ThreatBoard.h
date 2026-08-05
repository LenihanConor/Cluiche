#pragma once
#ifndef DIA_SENSOR_THREATBOARD_H
#define DIA_SENSOR_THREATBOARD_H

#include <DiaEntity/Entity.h>

namespace Dia::Sensor {

// ThreatBoard — AI concept struct written to the entity blackboard by
// DefaultSensorBlackboardAdapter.  Pure data; no DiaBlackboard dependency (AC-12).
struct ThreatBoard {
    Dia::Entity::Entity nearestThreat;         // invalid handle if no threat in range
    int                 threatCount   = 0;
    bool                underAttack   = false;
    int                 lastHitTime   = -1;    // frame number of last damage event; -1 = never
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_THREATBOARD_H
