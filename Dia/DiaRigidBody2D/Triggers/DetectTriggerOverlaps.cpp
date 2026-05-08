#include "DiaRigidBody2D/Triggers/DetectTriggerOverlaps.h"

#include "DiaRigidBody2D/WorldShapeUtil.h"
#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"

namespace Dia::RigidBody2D {

static bool ShouldOverlap(const TriggerVolume2D& trigger, const Body2DBase& body)
{
    return (trigger.GetLayer() & body.GetMask()) != 0 && (body.GetLayer() & trigger.GetMask()) != 0;
}

static bool NarrowOverlap(const TriggerVolume2D* trigger, const Body2DBase* body)
{
    if (trigger->GetCircleShape() && body->GetCircleShape())
    {
        Dia::Geometry2D::Circle wT = ComputeWorldCircle(trigger);
        Dia::Geometry2D::Circle wB = ComputeWorldCircle(body);
        return Dia::Geometry2D::IntersectionTests::IsIntersecting(wT, wB).IsIntersecting();
    }

    Dia::Geometry2D::AARect aabbT = ComputeWorldAABB(trigger);
    if (body->GetCircleShape())
    {
        Dia::Geometry2D::Circle wB = ComputeWorldCircle(body);
        return Dia::Geometry2D::IntersectionTests::IsIntersecting(aabbT, wB).IsIntersecting();
    }

    Dia::Geometry2D::AARect aabbB = ComputeWorldAABB(body);
    return Dia::Geometry2D::IntersectionTests::IsIntersecting(aabbT, aabbB).IsIntersecting();
}

void DetectTriggerOverlaps(
    Dia::Core::Containers::DynamicArrayC<TriggerVolume2D*, kMaxTriggerVolumes>&  triggers,
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>&         pointBodies,
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>&         rigidBodies,
    Dia::Geometry2D::ISpatialStructure<Body2DBase*>*                              broadPhase,
    Dia::Core::Containers::HashTable<TriggerPairKey, bool>&                      activePairs,
    Dia::Core::Containers::DynamicArrayC<TriggerEvent, kMaxTriggerEvents>&       outEvents,
    Dia::Core::ObserverSubject&                                                  subject)
{
    outEvents.RemoveAll();

    Dia::Core::Containers::HashTable<TriggerPairKey, bool> currentPairs;
    currentPairs.SetSize(kMaxTriggerEvents, kMaxTriggerEvents * 2);

    if (broadPhase)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Body2DBase*>, Dia::Geometry2D::kMaxQueryResults> candidates;

        for (unsigned int ti = 0; ti < triggers.Size(); ++ti)
        {
            TriggerVolume2D* trigger = triggers[ti];
            Dia::Geometry2D::AARect triggerAABB = ComputeWorldAABB(trigger);

            candidates.RemoveAll();
            broadPhase->QueryRegion(triggerAABB, candidates);

            for (unsigned int ci = 0; ci < candidates.Size(); ++ci)
            {
                Body2DBase* const* resolved = broadPhase->Resolve(candidates[ci]);
                if (!resolved || !*resolved) continue;
                Body2DBase* body = *resolved;

                if (!ShouldOverlap(*trigger, *body)) continue;
                if (!NarrowOverlap(trigger, body)) continue;

                TriggerPairKey key(trigger->GetUniqueId(), body->GetUniqueId());
                if (currentPairs.ContainsKey(key)) continue;
                currentPairs.Add(key, true);

                bool wasActive = activePairs.ContainsKey(key);

                TriggerEvent evt;
                evt.trigger = trigger;
                evt.body    = body;
                evt.type    = wasActive ? TriggerEventType::kStay : TriggerEventType::kEnter;

                if (!outEvents.IsFull())
                    outEvents.Add(evt);
            }
        }
    }
    else
    {
        // No broadphase — brute force trigger vs all bodies
        for (unsigned int ti = 0; ti < triggers.Size(); ++ti)
        {
            TriggerVolume2D* trigger = triggers[ti];
            auto checkBody = [&](Body2DBase* body)
            {
                if (!ShouldOverlap(*trigger, *body)) return;
                if (!body->GetCircleShape() && !body->GetPolyShape()) return;
                if (!NarrowOverlap(trigger, body)) return;

                TriggerPairKey key(trigger->GetUniqueId(), body->GetUniqueId());
                if (currentPairs.ContainsKey(key)) return;
                currentPairs.Add(key, true);

                bool wasActive = activePairs.ContainsKey(key);

                TriggerEvent evt;
                evt.trigger = trigger;
                evt.body    = body;
                evt.type    = wasActive ? TriggerEventType::kStay : TriggerEventType::kEnter;

                if (!outEvents.IsFull())
                    outEvents.Add(evt);
            };

            for (unsigned int i = 0; i < pointBodies.Size(); ++i)
                checkBody(pointBodies[i]);
            for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
                checkBody(rigidBodies[i]);
        }
    }

    // Emit exit events for pairs that were active last frame but not this frame
    {
        auto iter    = activePairs.Begin();
        auto iterEnd = activePairs.End();
        while (iter != iterEnd)
        {
            if (!currentPairs.ContainsKey(iter.Key()))
            {
                TriggerEvent evt;
                evt.type    = TriggerEventType::kExit;
                evt.trigger = nullptr;
                evt.body    = nullptr;
                if (!outEvents.IsFull())
                    outEvents.Add(evt);
            }
            ++iter;
        }
    }

    // Swap active pairs
    activePairs.RemoveAll();
    {
        auto iter    = currentPairs.Begin();
        auto iterEnd = currentPairs.End();
        while (iter != iterEnd)
        {
            activePairs.Add(iter.Key(), true);
            ++iter;
        }
    }

    if (outEvents.Size() > 0)
        subject.NotifyObservers(0);
}

} // namespace Dia::RigidBody2D
