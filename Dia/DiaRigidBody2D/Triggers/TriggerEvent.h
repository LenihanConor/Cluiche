#pragma once

namespace Dia::RigidBody2D {

class Body2DBase;
class TriggerVolume2D;

enum class TriggerEventType { kEnter, kStay, kExit };

struct TriggerEvent {
    TriggerEventType       type    = TriggerEventType::kEnter;
    const TriggerVolume2D* trigger = nullptr;
    const Body2DBase*      body    = nullptr;
};

} // namespace Dia::RigidBody2D
