#include "DiaRigidBody2D/World/PhysicsWorld.h"

#include "DiaRigidBody2D/Integration/Integration.h"
#include "DiaRigidBody2D/Integration/UpdateSleepTimers.h"
#include "DiaRigidBody2D/Detection/DetectCollisions.h"
#include "DiaRigidBody2D/Response/ResolveCollisions.h"
#include "DiaRigidBody2D/Constraints/ConstraintSolver.h"
#include "DiaRigidBody2D/Events/EmitCollisionEvents.h"
#include "DiaRigidBody2D/Triggers/DetectTriggerOverlaps.h"
#include "DiaRigidBody2D/WorldShapeUtil.h"
#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/Ray.h"
#include "DiaCore/Core/Assert.h"

namespace Dia::RigidBody2D {

PhysicsWorld::PhysicsWorld(const WorldDef& def)
    : mDef(def)
    , mAccumulator(0.0f)
    , mStepCount(0)
    , mNextBodyUid(1)
{
    mActivePairs.SetSize(kMaxContacts, kMaxContacts * 2);
    mActiveTriggerPairs.SetSize(kMaxTriggerEvents, kMaxTriggerEvents * 2);
}

PhysicsWorld::~PhysicsWorld()
{
    for (unsigned int i = 0; i < mPointBodies.Size(); ++i)
        delete mPointBodies[i];
    for (unsigned int i = 0; i < mRigidBodies.Size(); ++i)
        delete mRigidBodies[i];
    for (unsigned int i = 0; i < mConstraints.Size(); ++i)
        delete mConstraints[i];
    for (unsigned int i = 0; i < mTriggerVolumes.Size(); ++i)
        delete mTriggerVolumes[i];
}

// ---------------------------------------------------------------------------
// Body management
// ---------------------------------------------------------------------------

PointBody2D* PhysicsWorld::AddPointBody(const PointBodyDef& def)
{
    DIA_ASSERT(!mPointBodies.IsFull(), "PointBody pool is full");
    PointBody2D* body = new PointBody2D(def);
    body->SetUniqueId(mNextBodyUid++);
    mPointBodies.Add(body);

    Dia::Core::Handle<Body2DBase*> handle;
    if (mDef.broadPhase && (body->GetCircleShape() || body->GetPolyShape()))
    {
        Dia::Geometry2D::AARect aabb = ComputeWorldAABB(body);
        handle = mDef.broadPhase->Insert(static_cast<Body2DBase*>(body), aabb);
    }
    mPointHandles.Add(handle);

    return body;
}

RigidBody2D* PhysicsWorld::AddRigidBody(const RigidBodyDef& def)
{
    DIA_ASSERT(!mRigidBodies.IsFull(), "RigidBody pool is full");
    RigidBody2D* body = new RigidBody2D(def);
    body->SetUniqueId(mNextBodyUid++);
    mRigidBodies.Add(body);

    Dia::Core::Handle<Body2DBase*> handle;
    if (mDef.broadPhase && (body->GetCircleShape() || body->GetPolyShape()))
    {
        Dia::Geometry2D::AARect aabb = ComputeWorldAABB(body);
        handle = mDef.broadPhase->Insert(static_cast<Body2DBase*>(body), aabb);
    }
    mRigidHandles.Add(handle);

    return body;
}

void PhysicsWorld::RemovePointBody(PointBody2D* body)
{
    for (unsigned int i = 0; i < mPointBodies.Size(); ++i)
    {
        if (mPointBodies[i] == body)
        {
            FlushBodyReferences(body);
            if (mDef.broadPhase && mPointHandles[i].IsValid())
                mDef.broadPhase->Remove(mPointHandles[i]);
            delete body;
            mPointBodies.RemoveAt(i);
            mPointHandles.RemoveAt(i);
            return;
        }
    }
}

void PhysicsWorld::RemoveRigidBody(RigidBody2D* body)
{
    for (unsigned int i = 0; i < mRigidBodies.Size(); ++i)
    {
        if (mRigidBodies[i] == body)
        {
            FlushBodyReferences(body);

            // Remove any constraints referencing this body before deleting it
            for (unsigned int ci = mConstraints.Size(); ci > 0; --ci)
            {
                IConstraint* c = mConstraints[ci - 1];
                if (c->InvolvesBody(body))
                {
                    RigidBody2D* other = (c->GetBodyA() == body) ? c->GetBodyB() : c->GetBodyA();
                    if (other) other->RemoveConstraint(c);
                    delete c;
                    mConstraints.RemoveAt(ci - 1);
                }
            }

            if (mDef.broadPhase && mRigidHandles[i].IsValid())
                mDef.broadPhase->Remove(mRigidHandles[i]);
            delete body;
            mRigidBodies.RemoveAt(i);
            mRigidHandles.RemoveAt(i);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Trigger volume management
// ---------------------------------------------------------------------------

TriggerVolume2D* PhysicsWorld::AddTriggerVolume(const TriggerVolumeDef& def)
{
    DIA_ASSERT(!mTriggerVolumes.IsFull(), "TriggerVolume pool is full");
    TriggerVolume2D* trigger = new TriggerVolume2D(def);
    trigger->SetUniqueId(mNextBodyUid++);
    mTriggerVolumes.Add(trigger);
    return trigger;
}

void PhysicsWorld::RemoveTriggerVolume(TriggerVolume2D* trigger)
{
    for (unsigned int i = 0; i < mTriggerVolumes.Size(); ++i)
    {
        if (mTriggerVolumes[i] == trigger)
        {
            // Flush trigger events referencing this trigger
            for (unsigned int ei = mLastTriggerEvents.Size(); ei > 0; --ei)
            {
                if (mLastTriggerEvents[ei - 1].trigger == trigger)
                    mLastTriggerEvents.RemoveAt(ei - 1);
            }

            // Flush active trigger pairs
            Dia::Core::Containers::DynamicArrayC<TriggerPairKey, kMaxTriggerEvents> keysToRemove;
            {
                auto iter    = mActiveTriggerPairs.Begin();
                auto iterEnd = mActiveTriggerPairs.End();
                while (iter != iterEnd)
                {
                    if (iter.Key().triggerUid == trigger->GetUniqueId())
                        keysToRemove.Add(iter.Key());
                    ++iter;
                }
            }
            for (unsigned int ki = 0; ki < keysToRemove.Size(); ++ki)
                mActiveTriggerPairs.Remove(keysToRemove[ki]);

            delete trigger;
            mTriggerVolumes.RemoveAt(i);
            return;
        }
    }
}

int PhysicsWorld::GetTriggerVolumeCount() const
{
    return static_cast<int>(mTriggerVolumes.Size());
}

int PhysicsWorld::GetPointBodyCount() const
{
    return static_cast<int>(mPointBodies.Size());
}

int PhysicsWorld::GetRigidBodyCount() const
{
    return static_cast<int>(mRigidBodies.Size());
}

// ---------------------------------------------------------------------------
// Constraint management
// ---------------------------------------------------------------------------

IConstraint* PhysicsWorld::AddConstraint(IConstraint* constraint)
{
    DIA_ASSERT(constraint != nullptr, "Cannot add null constraint");
    DIA_ASSERT(!mConstraints.IsFull(), "Constraint pool is full");
    mConstraints.Add(constraint);

    if (constraint->GetBodyA()) constraint->GetBodyA()->AddConstraint(constraint);
    if (constraint->GetBodyB()) constraint->GetBodyB()->AddConstraint(constraint);

    return constraint;
}

void PhysicsWorld::RemoveConstraint(IConstraint* constraint)
{
    for (unsigned int i = 0; i < mConstraints.Size(); ++i)
    {
        if (mConstraints[i] == constraint)
        {
            if (constraint->GetBodyA()) constraint->GetBodyA()->RemoveConstraint(constraint);
            if (constraint->GetBodyB()) constraint->GetBodyB()->RemoveConstraint(constraint);
            delete constraint;
            mConstraints.RemoveAt(i);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Flush all references to a body about to be deleted
// ---------------------------------------------------------------------------

void PhysicsWorld::FlushBodyReferences(const Body2DBase* body)
{
    for (unsigned int i = mLastContacts.Size(); i > 0; --i)
    {
        const Contact& c = mLastContacts[i - 1];
        if (c.bodyA == body || c.bodyB == body)
            mLastContacts.RemoveAt(i - 1);
    }

    for (unsigned int i = mLastCollisionEvents.Size(); i > 0; --i)
    {
        const CollisionEvent& e = mLastCollisionEvents[i - 1];
        if (e.bodyA == body || e.bodyB == body)
            mLastCollisionEvents.RemoveAt(i - 1);
    }

    Dia::Core::Containers::DynamicArrayC<BodyPairKey, kMaxContacts> keysToRemove;
    {
        auto iter    = mActivePairs.Begin();
        auto iterEnd = mActivePairs.End();
        while (iter != iterEnd)
        {
            if (iter.Key().ContainsBody(body))
                keysToRemove.Add(iter.Key());
            ++iter;
        }
    }
    for (unsigned int i = 0; i < keysToRemove.Size(); ++i)
        mActivePairs.Remove(keysToRemove[i]);
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

void PhysicsWorld::Update(float deltaTime)
{
    mAccumulator += deltaTime;
    int steps = 0;
    while (mAccumulator >= mDef.fixedTimestep && steps < mDef.maxSubSteps)
    {
        StepOnce();
        mAccumulator -= mDef.fixedTimestep;
        ++steps;
        ++mStepCount;
    }
}

void PhysicsWorld::StepOnce()
{
    const float dt = mDef.fixedTimestep;

    IntegrateLinearForces(mPointBodies, mDef.gravity, dt);
    IntegrateLinearForces(mRigidBodies, mDef.gravity, dt);
    IntegrateAngularForces(mRigidBodies, dt);

    UpdateBroadPhase();
    DetectCollisions(mPointBodies, mRigidBodies, mDef.broadPhase, mLastContacts);
    ResolveCollisions(mLastContacts, mDef.responseConfig, dt);
    SolveConstraints(mConstraints, mDef.constraintConfig, dt);

    IntegrateLinearVelocities(mPointBodies, dt);
    IntegrateLinearVelocities(mRigidBodies, dt);
    IntegrateAngularVelocities(mRigidBodies, dt);

    EmitCollisionEvents(mLastContacts, mActivePairs, mLastCollisionEvents, mCollisionEvents);
    DetectTriggerOverlaps(mTriggerVolumes, mPointBodies, mRigidBodies, mDef.broadPhase,
                          mActiveTriggerPairs, mLastTriggerEvents, mTriggerEventSubject);
    ClearForceAccumulators(mPointBodies);
    ClearForceAccumulators(mRigidBodies);

    UpdateSleepTimers(mPointBodies, dt,
        mDef.sleepLinearThreshold, mDef.sleepTimeThreshold);
    UpdateSleepTimers(mRigidBodies, dt,
        mDef.sleepLinearThreshold, mDef.sleepAngularThreshold, mDef.sleepTimeThreshold);
}

void PhysicsWorld::UpdateBroadPhase()
{
    if (!mDef.broadPhase) return;

    for (unsigned int i = 0; i < mPointBodies.Size(); ++i)
    {
        PointBody2D* body = mPointBodies[i];
        if (!mPointHandles[i].IsValid()) continue;
        if (body->GetBodyType() == BodyType::kStatic) continue;
        Dia::Geometry2D::AARect aabb = ComputeWorldAABB(body);
        mDef.broadPhase->Update(mPointHandles[i], aabb);
    }
    for (unsigned int i = 0; i < mRigidBodies.Size(); ++i)
    {
        RigidBody2D* body = mRigidBodies[i];
        if (!mRigidHandles[i].IsValid()) continue;
        if (body->GetBodyType() == BodyType::kStatic) continue;
        Dia::Geometry2D::AARect aabb = ComputeWorldAABB(body);
        mDef.broadPhase->Update(mRigidHandles[i], aabb);
    }
}

// ---------------------------------------------------------------------------
// World properties
// ---------------------------------------------------------------------------

void PhysicsWorld::SetGravity(const Dia::Maths::Vector2D& gravity)
{
    mDef.gravity = gravity;
}

const Dia::Maths::Vector2D& PhysicsWorld::GetGravity() const
{
    return mDef.gravity;
}

float PhysicsWorld::GetFixedTimestep() const
{
    return mDef.fixedTimestep;
}

int PhysicsWorld::GetStepCount() const
{
    return mStepCount;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& PhysicsWorld::GetPointBodies() const
{
    return mPointBodies;
}

const Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& PhysicsWorld::GetRigidBodies() const
{
    return mRigidBodies;
}

const Dia::Core::Containers::DynamicArrayC<IConstraint*, kMaxConstraints>& PhysicsWorld::GetConstraints() const
{
    return mConstraints;
}

const Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>& PhysicsWorld::GetLastContacts() const
{
    return mLastContacts;
}

const Dia::Core::Containers::DynamicArrayC<CollisionEvent, kMaxCollisionEvents>& PhysicsWorld::GetLastCollisionEvents() const
{
    return mLastCollisionEvents;
}

const Dia::Core::Containers::DynamicArrayC<TriggerVolume2D*, kMaxTriggerVolumes>& PhysicsWorld::GetTriggerVolumes() const
{
    return mTriggerVolumes;
}

const Dia::Core::Containers::DynamicArrayC<TriggerEvent, kMaxTriggerEvents>& PhysicsWorld::GetLastTriggerEvents() const
{
    return mLastTriggerEvents;
}

Dia::Core::ObserverSubject& PhysicsWorld::GetCollisionEvents()
{
    return mCollisionEvents;
}

Dia::Core::ObserverSubject& PhysicsWorld::GetTriggerEvents()
{
    return mTriggerEventSubject;
}

// ---------------------------------------------------------------------------
// Spatial queries
// ---------------------------------------------------------------------------

bool PhysicsWorld::Raycast(const Dia::Geometry2D::Ray& ray, RaycastHit& outHit) const
{
    if (!mDef.broadPhase) return false;

    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Body2DBase*>, Dia::Geometry2D::kMaxQueryResults> candidates;
    mDef.broadPhase->QueryRay(ray, candidates);

    float closest = 1e30f;
    outHit.body   = nullptr;

    for (unsigned int i = 0; i < candidates.Size(); ++i)
    {
        Body2DBase* const* resolved = mDef.broadPhase->Resolve(candidates[i]);
        if (!resolved || !*resolved) continue;
        const Body2DBase* body = *resolved;

        bool hit = false;
        float dist = 0.0f;
        Dia::Maths::Vector2D hitPt, hitNormal;

        Dia::Geometry2D::RaycastHit rh;
        if (body->GetCircleShape())
        {
            Dia::Geometry2D::Circle worldCircle = ComputeWorldCircle(body);
            if (Dia::Geometry2D::Raycast::CastCircle(ray, worldCircle, rh))
            {
                hit      = true;
                dist     = rh.distance;
                hitPt    = rh.point;
                hitNormal = rh.normal;
            }
        }
        else
        {
            Dia::Geometry2D::AARect worldAABB = ComputeWorldAABB(body);
            if (Dia::Geometry2D::Raycast::CastAARect(ray, worldAABB, rh))
            {
                hit      = true;
                dist     = rh.distance;
                hitPt    = rh.point;
                hitNormal = rh.normal;
            }
        }

        if (hit && dist < closest)
        {
            closest          = dist;
            outHit.body      = body;
            outHit.point     = hitPt;
            outHit.normal    = hitNormal;
            outHit.distance  = dist;
        }
    }

    return outHit.body != nullptr;
}

void PhysicsWorld::QueryRegion(
    const Dia::Geometry2D::AARect& region,
    Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults>& outBodies) const
{
    outBodies.RemoveAll();
    if (!mDef.broadPhase) return;

    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Body2DBase*>, Dia::Geometry2D::kMaxQueryResults> candidates;
    mDef.broadPhase->QueryRegion(region, candidates);

    for (unsigned int i = 0; i < candidates.Size(); ++i)
    {
        Body2DBase* const* resolved = mDef.broadPhase->Resolve(candidates[i]);
        if (!resolved || !*resolved) continue;
        const Body2DBase* body = *resolved;

        bool confirmed = false;
        if (body->GetCircleShape())
        {
            Dia::Geometry2D::Circle worldCircle = ComputeWorldCircle(body);
            confirmed = Dia::Geometry2D::IntersectionTests::IsIntersecting(region, worldCircle).IsIntersecting();
        }
        else
        {
            Dia::Geometry2D::AARect worldAABB = ComputeWorldAABB(body);
            confirmed = Dia::Geometry2D::IntersectionTests::IsIntersecting(region, worldAABB).IsIntersecting();
        }

        if (confirmed && !outBodies.IsFull())
            outBodies.Add(body);
    }
}

void PhysicsWorld::QueryCircle(
    const Dia::Geometry2D::Circle& circle,
    Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults>& outBodies) const
{
    outBodies.RemoveAll();
    if (!mDef.broadPhase) return;

    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Body2DBase*>, Dia::Geometry2D::kMaxQueryResults> candidates;
    mDef.broadPhase->QueryCircle(circle, candidates);

    for (unsigned int i = 0; i < candidates.Size(); ++i)
    {
        Body2DBase* const* resolved = mDef.broadPhase->Resolve(candidates[i]);
        if (!resolved || !*resolved) continue;
        const Body2DBase* body = *resolved;

        bool confirmed = false;
        if (body->GetCircleShape())
        {
            Dia::Geometry2D::Circle worldCircle = ComputeWorldCircle(body);
            confirmed = Dia::Geometry2D::IntersectionTests::IsIntersecting(worldCircle, circle).IsIntersecting();
        }
        else
        {
            Dia::Geometry2D::AARect worldAABB = ComputeWorldAABB(body);
            confirmed = Dia::Geometry2D::IntersectionTests::IsIntersecting(worldAABB, circle).IsIntersecting();
        }

        if (confirmed && !outBodies.IsFull())
            outBodies.Add(body);
    }
}

} // namespace Dia::RigidBody2D
