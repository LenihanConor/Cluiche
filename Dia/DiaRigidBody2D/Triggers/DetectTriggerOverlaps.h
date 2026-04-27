#pragma once

#include "DiaRigidBody2D/Triggers/TriggerVolume2D.h"
#include "DiaRigidBody2D/Triggers/TriggerEvent.h"
#include "DiaRigidBody2D/Triggers/TriggerPairKey.h"
#include "DiaRigidBody2D/Bodies/PointBody2D.h"
#include "DiaRigidBody2D/Bodies/RigidBody2D.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaGeometry2D/Spatial/ISpatialStructure.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Containers/HashTables/HashTable.h"
#include "DiaCore/Architecture/Observer.h"

namespace Dia::RigidBody2D {

void DetectTriggerOverlaps(
    Dia::Core::Containers::DynamicArrayC<TriggerVolume2D*, kMaxTriggerVolumes>&  triggers,
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>&         pointBodies,
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>&         rigidBodies,
    Dia::Geometry2D::ISpatialStructure<Body2DBase*>*                              broadPhase,
    Dia::Core::Containers::HashTable<TriggerPairKey, bool>&                      activePairs,
    Dia::Core::Containers::DynamicArrayC<TriggerEvent, kMaxTriggerEvents>&       outEvents,
    Dia::Core::ObserverSubject&                                                  subject);

} // namespace Dia::RigidBody2D
