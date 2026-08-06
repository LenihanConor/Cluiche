#pragma once
#ifndef DIA_SENSOR_SENSORMODULE_H
#define DIA_SENSOR_SENSORMODULE_H

#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaSensor/SoundEventList.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaSensor/SoundType.h>

namespace Dia { namespace Entity { class Domain; } }
namespace Dia { namespace EntitySpatial { class EntitySpatialModule; } }
namespace Dia { namespace Observation { namespace Metric {
    class Counter;
} } }

namespace Dia::Sensor {

// SensorModule — ApplicationFlow::Module on SimPU.
// Owns the frame-local SoundEventList.
// Drives sensor tick countdowns and the two-pass frame loop.
//
// DoUpdate internally calls in order:
//   1. Update()       — clears SoundEventList; ticks all sensor components
//   2. RunAdapters()  — distils SensorResultsComponent -> blackboard (AC-7)
class SensorModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kInstanceId;

    // domain:        the entity domain this module operates on
    // spatialModule: the EntitySpatialModule for spatial queries
    SensorModule(Dia::Entity::Domain& domain,
                 Dia::EntitySpatial::EntitySpatialModule& spatialModule);

    // Emit a sound this frame. Called by combat/ability/footstep systems.
    // Appended to the frame-local SoundEventList. Cleared at start of next Update() (AC-10).
    void EmitSound(const Dia::Maths::Vector2D& position, SoundType type, float radius);

    // Read-only access to the frame-local sound event list (mainly for tests).
    const SoundEventList& GetSoundEventList() const { return mSoundEventList; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void                              DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    // --- Two-pass frame loop ---

    // Pass 1: clear SoundEventList; tick all sensor components at their configured rates (AC-6, AC-10).
    void Update(int frameNumber);

    // Pass 2: call SensorBlackboardAdapter::Distil on every entity that carries one (AC-8).
    void RunAdapters(int frameNumber);

    // --- Per-entity tick countdowns ---
    // Indexed by entity slot (entity.GetIndex()); one struct per entity.
    struct EntityCountdowns
    {
        int sight     = 0;
        int proximity = 0;
        int damage    = 0;
        int sound     = 0;
    };

    static constexpr unsigned int kMaxTrackedEntities = Dia::Entity::kMaxEntitiesPerDomain;

    Dia::Core::Containers::DynamicArrayC<EntityCountdowns, kMaxTrackedEntities> mCountdowns;

    Dia::Entity::Domain&                     mDomain;
    Dia::EntitySpatial::EntitySpatialModule& mSpatialModule;
    SoundEventList                           mSoundEventList;
    int                                      mFrameNumber = 0;

    // Per-frame tick counts (updated in Update/RunAdapters, read for log + metrics)
    int mLastSightTicks     = 0;
    int mLastProximityTicks = 0;
    int mLastDamageTicks    = 0;
    int mLastSoundTicks     = 0;
    int mLastAdapterRuns    = 0;

    // DiaObservation metric pointers (valid between DoStart/DoStop)
    Dia::Observation::Metric::Counter* mSightTicksCounter     = nullptr;
    Dia::Observation::Metric::Counter* mProximityTicksCounter = nullptr;
    Dia::Observation::Metric::Counter* mDamageTicksCounter    = nullptr;
    Dia::Observation::Metric::Counter* mSoundTicksCounter     = nullptr;
    Dia::Observation::Metric::Counter* mAdaptersCounter       = nullptr;
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_SENSORMODULE_H
