#pragma once

namespace Dia::RigidBody2D {

enum class BodyType   { kDynamic, kStatic, kKinematic };
enum class SleepState { kAwake, kSleeping };
enum class ShapeKind  { kNone, kCircle, kPoly };

} // namespace Dia::RigidBody2D
