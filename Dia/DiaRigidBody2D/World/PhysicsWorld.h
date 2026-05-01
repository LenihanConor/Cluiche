#pragma once

#include "DiaRigidBody2D/World/WorldDef.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaRigidBody2D/World/BodyPairKey.h"
#include "DiaRigidBody2D/Bodies/PointBody2D.h"
#include "DiaRigidBody2D/Bodies/RigidBody2D.h"
#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include "DiaRigidBody2D/Detection/Contact.h"
#include "DiaRigidBody2D/Events/CollisionEvent.h"
#include "DiaRigidBody2D/Events/EmitCollisionEvents.h"
#include "DiaRigidBody2D/Triggers/TriggerVolume2D.h"
#include "DiaRigidBody2D/Triggers/TriggerEvent.h"
#include "DiaRigidBody2D/Triggers/TriggerPairKey.h"
#include "DiaRigidBody2D/Queries/RaycastHit.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Containers/HashTables/HashTable.h"
#include "DiaCore/Containers/Handle.h"
#include "DiaCore/Architecture/Observer.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Ray.h"

namespace Dia::RigidBody2D {

class PhysicsWorld {
public:
    explicit PhysicsWorld(const WorldDef& def);
    ~PhysicsWorld();

    // --- Body management ---
    PointBody2D* AddPointBody(const PointBodyDef& def);
    RigidBody2D* AddRigidBody(const RigidBodyDef& def);
    void         RemovePointBody(PointBody2D* body);
    void         RemoveRigidBody(RigidBody2D* body);
    int          GetPointBodyCount() const;
    int          GetRigidBodyCount() const;

    // --- Trigger volume management ---
    TriggerVolume2D* AddTriggerVolume(const TriggerVolumeDef& def);
    void             RemoveTriggerVolume(TriggerVolume2D* trigger);
    int              GetTriggerVolumeCount() const;

    // --- Constraint management (takes ownership) ---
    IConstraint* AddConstraint(IConstraint* constraint);
    void         RemoveConstraint(IConstraint* constraint);

    // --- Simulation ---
    void Update(float deltaTime);

    // --- World properties ---
    void                        SetGravity(const Dia::Maths::Vector2D& gravity);
    const Dia::Maths::Vector2D& GetGravity() const;
    float                       GetFixedTimestep() const;
    int                         GetStepCount() const;  // Total steps run (for test counting)

    // --- Read accessors ---
    const Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& GetPointBodies() const;
    const Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& GetRigidBodies() const;
    const Dia::Core::Containers::DynamicArrayC<IConstraint*, kMaxConstraints>&  GetConstraints() const;
    const Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>&                   GetLastContacts() const;
    const Dia::Core::Containers::DynamicArrayC<CollisionEvent, kMaxCollisionEvents>&    GetLastCollisionEvents() const;
    const Dia::Core::Containers::DynamicArrayC<TriggerVolume2D*, kMaxTriggerVolumes>&  GetTriggerVolumes() const;
    const Dia::Core::Containers::DynamicArrayC<TriggerEvent, kMaxTriggerEvents>&       GetLastTriggerEvents() const;

    // --- Collision event observer ---
    Dia::Core::ObserverSubject& GetCollisionEvents();
    Dia::Core::ObserverSubject& GetTriggerEvents();

    // --- Spatial queries ---
    bool Raycast    (const Dia::Geometry2D::Ray&    ray,    RaycastHit& outHit)                                                                          const;
    void QueryRegion(const Dia::Geometry2D::AARect& region, Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults>& outBodies)              const;
    void QueryCircle(const Dia::Geometry2D::Circle& circle, Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults>& outBodies)              const;

private:
    void StepOnce();
    void UpdateBroadPhase();
    void FlushBodyReferences(const Body2DBase* body);

    WorldDef                                                                 mDef;
    float                                                                    mAccumulator;
    int                                                                      mStepCount;
    uint32_t                                                                 mNextBodyUid;
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>              mPointBodies;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Body2DBase*>, kMaxPointBodies> mPointHandles;
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>              mRigidBodies;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Body2DBase*>, kMaxRigidBodies> mRigidHandles;
    Dia::Core::Containers::DynamicArrayC<IConstraint*, kMaxConstraints>              mConstraints;
    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>                      mLastContacts;
    Dia::Core::Containers::DynamicArrayC<CollisionEvent, kMaxCollisionEvents>        mLastCollisionEvents;
    Dia::Core::ObserverSubject                                                       mCollisionEvents;
    Dia::Core::Containers::HashTable<BodyPairKey, CollisionPairState>                mActivePairs;
    Dia::Core::Containers::DynamicArrayC<TriggerVolume2D*, kMaxTriggerVolumes>       mTriggerVolumes;
    Dia::Core::Containers::DynamicArrayC<TriggerEvent, kMaxTriggerEvents>            mLastTriggerEvents;
    Dia::Core::ObserverSubject                                                       mTriggerEventSubject;
    Dia::Core::Containers::HashTable<TriggerPairKey, bool>                           mActiveTriggerPairs;
};

} // namespace Dia::RigidBody2D
