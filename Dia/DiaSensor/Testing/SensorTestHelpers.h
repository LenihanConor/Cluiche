#pragma once
#ifndef DIA_SENSOR_TESTING_SENSORTESTHELPERS_H
#define DIA_SENSOR_TESTING_SENSORTESTHELPERS_H

#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaSensor/SensorResultsComponent.h>
#include <DiaSensor/SensorResults.h>
#include <DiaSensor/SoundType.h>
#include <DiaSensor/ThreatBoard.h>
#include <DiaSensor/AwarenessBoard.h>
#include <DiaSensor/DefaultSensorBlackboardAdapter.h>
#include <DiaBlackboard/BlackboardComponent.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Json/external/json/json.h>
#include <gtest/gtest.h>
#include <cstdint>

namespace Dia { namespace Sensor { namespace Testing {

    // -------------------------------------------------------------------------
    // SpawnSensorEntity
    //
    // Creates an entity in `domain` with SpatialComponent + SensorResultsComponent.
    // The domain must have pools registered for both component types before calling.
    // The entity is queued but not yet live — call domain.EndOfFrame() to apply.
    //
    // Returns the entity handle.
    // -------------------------------------------------------------------------
    inline Dia::Entity::Entity SpawnSensorEntity(
        Dia::Entity::Domain&        domain,
        const Dia::Maths::Vector2D& pos,
        uint32_t                    layerMask = 0x01)
    {
        Dia::Entity::Entity e = domain.CreateEntity();

        Json::Value spatialCfg;
        spatialCfg["position"]["x"] = pos.x;
        spatialCfg["position"]["y"] = pos.y;
        spatialCfg["radius"]        = 1.0f;
        spatialCfg["layerMask"]     = layerMask & 0x7FFFFFFFu;
        domain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(e, spatialCfg);

        Json::Value resultsCfg;
        domain.QueueAddComponent<Dia::Sensor::SensorResultsComponent>(e, resultsCfg);

        return e;
    }

    // -------------------------------------------------------------------------
    // InjectSightResult
    //
    // Directly appends a SightResult into the entity's SensorResultsComponent.
    // The entity must have SensorResultsComponent already live (after EndOfFrame).
    // -------------------------------------------------------------------------
    inline void InjectSightResult(
        Dia::Entity::Domain&  domain,
        Dia::Entity::Entity   e,
        Dia::Entity::Entity   target,
        float                 distance,
        float                 angle,
        int                   timestamp)
    {
        Dia::Sensor::SensorResultsComponent* results =
            domain.GetComponent<Dia::Sensor::SensorResultsComponent>(e);
        if (results == nullptr) { return; }

        Dia::Sensor::SightResult sr;
        sr.entity    = target;
        sr.distance  = distance;
        sr.angle     = angle;
        sr.timestamp = timestamp;
        results->sightResults.Add(sr);
    }

    // -------------------------------------------------------------------------
    // InjectProximityResult
    //
    // Directly appends a ProximityResult into the entity's SensorResultsComponent.
    // -------------------------------------------------------------------------
    inline void InjectProximityResult(
        Dia::Entity::Domain&  domain,
        Dia::Entity::Entity   e,
        Dia::Entity::Entity   target,
        float                 distance)
    {
        Dia::Sensor::SensorResultsComponent* results =
            domain.GetComponent<Dia::Sensor::SensorResultsComponent>(e);
        if (results == nullptr) { return; }

        Dia::Sensor::ProximityResult pr;
        pr.entity   = target;
        pr.distance = distance;
        results->proximityResults.Add(pr);
    }

    // -------------------------------------------------------------------------
    // InjectDamageEvent
    //
    // Directly appends a DamageEvent into the entity's SensorResultsComponent.
    // -------------------------------------------------------------------------
    inline void InjectDamageEvent(
        Dia::Entity::Domain&  domain,
        Dia::Entity::Entity   e,
        Dia::Entity::Entity   source,
        float                 amount,
        int                   timestamp)
    {
        Dia::Sensor::SensorResultsComponent* results =
            domain.GetComponent<Dia::Sensor::SensorResultsComponent>(e);
        if (results == nullptr) { return; }

        Dia::Sensor::DamageEvent de;
        de.source    = source;
        de.amount    = amount;
        de.timestamp = timestamp;
        results->damageEvents.Add(de);
    }

    // -------------------------------------------------------------------------
    // InjectSoundEvent
    //
    // Directly appends a SoundEvent into the entity's SensorResultsComponent.
    // -------------------------------------------------------------------------
    inline void InjectSoundEvent(
        Dia::Entity::Domain&        domain,
        Dia::Entity::Entity         e,
        const Dia::Maths::Vector2D& pos,
        Dia::Sensor::SoundType      type,
        int                         timestamp)
    {
        Dia::Sensor::SensorResultsComponent* results =
            domain.GetComponent<Dia::Sensor::SensorResultsComponent>(e);
        if (results == nullptr) { return; }

        Dia::Sensor::SoundEvent se;
        se.position  = pos;
        se.type      = type;
        se.timestamp = timestamp;
        results->soundEvents.Add(se);
    }

    // -------------------------------------------------------------------------
    // AssertThreatBoard
    //
    // Reads ThreatBoard from a BlackboardComponent and asserts the given values.
    // The blackboard must have ThreatBoard registered (via BindBlackboard).
    // -------------------------------------------------------------------------
    inline void AssertThreatBoard(
        Dia::Blackboard::BlackboardComponent& bc,
        int   expectedThreatCount,
        bool  expectedUnderAttack)
    {
        const Dia::Blackboard::Blackboard& bb = bc.GetBlackboard();
        const Dia::Sensor::ThreatBoard* tb =
            bb.TryGet<Dia::Sensor::ThreatBoard>(
                Dia::Sensor::DefaultSensorBlackboardAdapter::kThreatBoardKey);
        ASSERT_NE(tb, nullptr) << "ThreatBoard slot not registered on blackboard";
        EXPECT_EQ(tb->threatCount,  expectedThreatCount);
        EXPECT_EQ(tb->underAttack,  expectedUnderAttack);
    }

    // -------------------------------------------------------------------------
    // AssertAwarenessBoard
    //
    // Reads AwarenessBoard from a BlackboardComponent and asserts the given values.
    // The blackboard must have AwarenessBoard registered (via BindBlackboard).
    // -------------------------------------------------------------------------
    inline void AssertAwarenessBoard(
        Dia::Blackboard::BlackboardComponent& bc,
        int                         expectedKnownEnemyCount,
        Dia::Sensor::AlertLevel     expectedAlertLevel)
    {
        const Dia::Blackboard::Blackboard& bb = bc.GetBlackboard();
        const Dia::Sensor::AwarenessBoard* ab =
            bb.TryGet<Dia::Sensor::AwarenessBoard>(
                Dia::Sensor::DefaultSensorBlackboardAdapter::kAwarenessBoardKey);
        ASSERT_NE(ab, nullptr) << "AwarenessBoard slot not registered on blackboard";
        EXPECT_EQ(ab->knownEnemyCount, expectedKnownEnemyCount);
        EXPECT_EQ(ab->alertLevel,      expectedAlertLevel);
    }

}}} // namespace Dia::Sensor::Testing

#endif // DIA_SENSOR_TESTING_SENSORTESTHELPERS_H
