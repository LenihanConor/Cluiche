#include <DiaSensor/DefaultSensorBlackboardAdapter.h>
#include <DiaSensor/SensorResultsComponent.h>
#include <DiaSensor/SensorResults.h>
#include <DiaBlackboard/BlackboardComponent.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaCore/Core/Assert.h>

// Serialize free function — no serializable FIELDs on this component.
DIA_SERIALIZE(Dia::Sensor::DefaultSensorBlackboardAdapter, Dia::Sensor::DefaultSensorBlackboardAdapter::kVersion)
DIA_SERIALIZE_END

namespace Dia::Sensor {

// ---------------------------------------------------------------------------
// Static slot key definitions
// ---------------------------------------------------------------------------
const Dia::Core::StringCRC DefaultSensorBlackboardAdapter::kThreatBoardKey("threat-board");
const Dia::Core::StringCRC DefaultSensorBlackboardAdapter::kAwarenessBoardKey("awareness-board");

// ---------------------------------------------------------------------------
// DIA_COMPONENT_REGISTER
// ---------------------------------------------------------------------------
DIA_COMPONENT_REGISTER(DefaultSensorBlackboardAdapter, "default-sensor-blackboard-adapter",
    false, false,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

// ---------------------------------------------------------------------------
// BindBlackboard
// ---------------------------------------------------------------------------
void DefaultSensorBlackboardAdapter::BindBlackboard(
    Dia::Blackboard::BlackboardComponent& blackboardComponent)
{
    mBoundBlackboard = &blackboardComponent;

    Dia::Blackboard::Blackboard& bb = blackboardComponent.GetBlackboard();
    bb.Register<ThreatBoard>(kThreatBoardKey);
    bb.Register<AwarenessBoard>(kAwarenessBoardKey);
}

// ---------------------------------------------------------------------------
// GetBlackboard
// ---------------------------------------------------------------------------
Dia::Blackboard::BlackboardComponent& DefaultSensorBlackboardAdapter::GetBlackboard()
{
    DIA_ASSERT(mBoundBlackboard != nullptr,
        "DefaultSensorBlackboardAdapter::GetBlackboard() called before BindBlackboard()");
    return *mBoundBlackboard;
}

// ---------------------------------------------------------------------------
// Distil
// ---------------------------------------------------------------------------
void DefaultSensorBlackboardAdapter::Distil(
    const SensorResultsComponent& results,
    Dia::Blackboard::BlackboardComponent& blackboard,
    int frameNumber) const
{
    Dia::Blackboard::Blackboard& bb = blackboard.GetBlackboard();

    ThreatBoard&    threat    = bb.Get<ThreatBoard>(kThreatBoardKey);
    AwarenessBoard& awareness = bb.Get<AwarenessBoard>(kAwarenessBoardKey);

    // Reset both boards each frame — fresh distillation, not accumulation.
    threat    = ThreatBoard{};
    awareness = AwarenessBoard{};

    // -----------------------------------------------------------------------
    // Sight results: nearest threat, threat count, known enemy count,
    // last known enemy position.
    // -----------------------------------------------------------------------
    float nearestDist = 3.4028235e+38f; // FLT_MAX without <cfloat>

    for (unsigned int i = 0; i < results.sightResults.Size(); ++i)
    {
        const SightResult& sr = results.sightResults[i];
        awareness.knownEnemyCount++;

        if (sr.distance < nearestDist)
        {
            nearestDist            = sr.distance;
            threat.nearestThreat   = sr.entity;
            // Note: SightResult carries distance + angle but no absolute position.
            // lastKnownEnemyPosition stays (0,0) until a full domain position is
            // available from the caller.  See AC-9 open design note.
        }
    }

    threat.threatCount = static_cast<int>(results.sightResults.Size());

    if (threat.threatCount > 0)
    {
        awareness.alertLevel = AlertLevel::kCombat;
    }

    // -----------------------------------------------------------------------
    // Proximity results: entities in range but not necessarily in sight.
    // Boost alert level if any proximity detections exist.
    // -----------------------------------------------------------------------
    for (unsigned int i = 0; i < results.proximityResults.Size(); ++i)
    {
        if (awareness.alertLevel == AlertLevel::kIdle)
        {
            awareness.alertLevel = AlertLevel::kAlert;
        }
    }

    // -----------------------------------------------------------------------
    // Damage events: underAttack flag and lastHitTime.
    // -----------------------------------------------------------------------
    for (unsigned int i = 0; i < results.damageEvents.Size(); ++i)
    {
        const DamageEvent& de = results.damageEvents[i];

        // Any damage received in the current or previous frame counts as active attack.
        if (de.timestamp >= frameNumber - 1)
        {
            threat.underAttack = true;
        }

        if (de.timestamp > threat.lastHitTime)
        {
            threat.lastHitTime = de.timestamp;
        }
    }
}

} // namespace Dia::Sensor
