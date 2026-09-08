#pragma once
#ifndef DIA_SENSOR_DEFAULTSENSORBLACKBOARDADAPTER_H
#define DIA_SENSOR_DEFAULTSENSORBLACKBOARDADAPTER_H

#include <DiaSensor/SensorBlackboardAdapter.h>
#include <DiaSensor/ThreatBoard.h>
#include <DiaSensor/AwarenessBoard.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Blackboard { class BlackboardComponent; } }

namespace Dia::Sensor {

// DefaultSensorBlackboardAdapter — concrete SensorBlackboardAdapter that writes
// ThreatBoard and AwarenessBoard slots on the entity's blackboard (AC-9).
//
// Usage:
//   1. Attach DefaultSensorBlackboardAdapter to the entity.
//   2. After attaching both this and BlackboardComponent, call BindBlackboard()
//      once to register the two blackboard slots.
//   3. SensorModule calls Distil() each frame — do NOT call it from sensor code (AC-8).
class DefaultSensorBlackboardAdapter : public SensorBlackboardAdapter {
    DIA_COMPONENT(DefaultSensorBlackboardAdapter, "default-sensor-blackboard-adapter", 1)

public:
    // Slot keys used to register / look up the boards on the blackboard.
    static const Dia::Core::StringCRC kThreatBoardKey;
    static const Dia::Core::StringCRC kAwarenessBoardKey;

    // Bind to the entity's BlackboardComponent.
    // Must be called once after both components are attached to the entity.
    // Registers ThreatBoard and AwarenessBoard slots on the blackboard.
    void BindBlackboard(Dia::Blackboard::BlackboardComponent& blackboardComponent);

    // SensorBlackboardAdapter overrides
    void Distil(const SensorResultsComponent& results,
                Dia::Blackboard::BlackboardComponent& blackboard,
                int frameNumber) const override;

    Dia::Blackboard::BlackboardComponent& GetBlackboard() override;

private:
    Dia::Blackboard::BlackboardComponent* mBoundBlackboard = nullptr;
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_DEFAULTSENSORBLACKBOARDADAPTER_H
