#include <DiaSensor/SensorModule.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>

#include <DiaEntity/Domain.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaSensor/SensorResultsComponent.h>
#include <DiaSensor/SightSensorComponent.h>
#include <DiaSensor/ProximitySensorComponent.h>
#include <DiaSensor/DamageSensorComponent.h>
#include <DiaSensor/DamageReceivedComponent.h>
#include <DiaSensor/SoundSensorComponent.h>
#include <DiaSensor/SensorBlackboardAdapter.h>

namespace Dia::Sensor {

const Dia::Core::StringCRC SensorModule::kInstanceId("sensor-module");

SensorModule::SensorModule(Dia::Entity::Domain& domain,
                           Dia::EntitySpatial::EntitySpatialModule& spatialModule)
    : SimModule(kInstanceId)
    , mDomain(domain)
    , mSpatialModule(spatialModule)
    , mSoundEventList()
    , mFrameNumber(0)
{
}

void SensorModule::EmitSound(const Dia::Maths::Vector2D& position, SoundType type, float radius)
{
    mSoundEventList.Emit(position, type, radius, mFrameNumber);
}

Dia::ApplicationFlow::StartResult SensorModule::DoStart()
{
    // Pre-fill the countdowns array to kMaxTrackedEntities zero-initialised entries.
    // EntityCountdowns is zero-initialised by its in-class defaults, so AddDefault is correct.
    for (unsigned int i = 0; i < kMaxTrackedEntities; ++i)
    {
        mCountdowns.AddDefault();
    }

    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    mSightTicksCounter     = reg.RegisterCounter(Dia::Core::StringCRC("sensor.ticks.sight_per_frame"));
    mProximityTicksCounter = reg.RegisterCounter(Dia::Core::StringCRC("sensor.ticks.proximity_per_frame"));
    mDamageTicksCounter    = reg.RegisterCounter(Dia::Core::StringCRC("sensor.ticks.damage_per_frame"));
    mSoundTicksCounter     = reg.RegisterCounter(Dia::Core::StringCRC("sensor.ticks.sound_per_frame"));
    mAdaptersCounter       = reg.RegisterCounter(Dia::Core::StringCRC("sensor.adapters.distilled_per_frame"));

    DIA_LOG_INFO("sensor", "SensorModule started — tracking up to %u entity slots", kMaxTrackedEntities);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void SensorModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    ++mFrameNumber;
    Update(mFrameNumber);
    RunAdapters(mFrameNumber);
    DIA_LOG_DEBUG("sensor", "frame %d — ticks: sight=%d prox=%d dmg=%d snd=%d adapters=%d",
        mFrameNumber, mLastSightTicks, mLastProximityTicks, mLastDamageTicks, mLastSoundTicks, mLastAdapterRuns);
}

Dia::ApplicationFlow::StopResult SensorModule::DoStop()
{
    mSightTicksCounter = mProximityTicksCounter = mDamageTicksCounter =
    mSoundTicksCounter = mAdaptersCounter = nullptr;

    DIA_LOG_INFO("sensor", "SensorModule stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void SensorModule::Update(int frameNumber)
{
    DIA_TRACE_ZONE("sensor.update", Dia::Observation::Trace::Category::kNone);

    // Reset per-frame tick counts.
    mLastSightTicks     = 0;
    mLastProximityTicks = 0;
    mLastDamageTicks    = 0;
    mLastSoundTicks     = 0;

    // AC-10: clear the sound event list BEFORE any sensor ticks this frame.
    mSoundEventList.Clear();

    // Default forward direction — SpatialComponent has no orientation field.
    static const Dia::Maths::Vector2D kDefaultForward(1.f, 0.f);

    // Iterate all entities that carry SensorResultsComponent.
    auto view = mDomain.Query<SensorResultsComponent>();
    for (auto entry : view)
    {
        Dia::Entity::Entity entity         = entry.entity;
        SensorResultsComponent& results    = *std::get<0>(entry.components);
        const uint32_t idx                 = entity.GetIndex();

        // Resolve owner position from SpatialComponent (if present).
        Dia::Maths::Vector2D ownerPosition(0.f, 0.f);
        const Dia::EntitySpatial::SpatialComponent* spatial =
            mDomain.GetComponent<Dia::EntitySpatial::SpatialComponent>(entity);
        if (spatial != nullptr)
        {
            ownerPosition = spatial->position;
        }

        EntityCountdowns& cd = mCountdowns[idx];

        // --- Sight sensor ---
        SightSensorComponent* sight =
            mDomain.GetComponent<SightSensorComponent>(entity);
        if (sight != nullptr)
        {
            --cd.sight;
            if (cd.sight <= 0)
            {
                sight->Tick(mSpatialModule, mDomain, ownerPosition, kDefaultForward,
                            results, frameNumber);
                cd.sight = sight->tickInterval;
                ++mLastSightTicks;
            }
        }

        // --- Proximity sensor ---
        ProximitySensorComponent* proximity =
            mDomain.GetComponent<ProximitySensorComponent>(entity);
        if (proximity != nullptr)
        {
            --cd.proximity;
            if (cd.proximity <= 0)
            {
                proximity->Tick(mSpatialModule, mDomain, ownerPosition,
                                results, frameNumber);
                cd.proximity = proximity->tickInterval;
                ++mLastProximityTicks;
            }
        }

        // --- Damage sensor ---
        DamageSensorComponent* damage =
            mDomain.GetComponent<DamageSensorComponent>(entity);
        if (damage != nullptr)
        {
            --cd.damage;
            if (cd.damage <= 0)
            {
                DamageReceivedComponent* damageReceived =
                    mDomain.GetComponent<DamageReceivedComponent>(entity);
                if (damageReceived != nullptr)
                {
                    damage->Tick(*damageReceived, results, frameNumber);
                }
                cd.damage = damage->tickInterval;
                ++mLastDamageTicks;
            }
        }

        // --- Sound sensor ---
        SoundSensorComponent* sound =
            mDomain.GetComponent<SoundSensorComponent>(entity);
        if (sound != nullptr)
        {
            --cd.sound;
            if (cd.sound <= 0)
            {
                sound->Tick(mSoundEventList, ownerPosition, results);
                cd.sound = sound->tickInterval;
                ++mLastSoundTicks;
            }
        }
    }

    if (mSightTicksCounter)
        mSightTicksCounter->Inc(static_cast<uint64_t>(mLastSightTicks));
    if (mProximityTicksCounter)
        mProximityTicksCounter->Inc(static_cast<uint64_t>(mLastProximityTicks));
    if (mDamageTicksCounter)
        mDamageTicksCounter->Inc(static_cast<uint64_t>(mLastDamageTicks));
    if (mSoundTicksCounter)
        mSoundTicksCounter->Inc(static_cast<uint64_t>(mLastSoundTicks));
}

void SensorModule::RunAdapters(int frameNumber)
{
    DIA_TRACE_ZONE("sensor.run_adapters", Dia::Observation::Trace::Category::kNone);

    mLastAdapterRuns = 0;

    // AC-8: SensorBlackboardAdapter::Distil() is called by this method ONLY —
    //       never from sensor component Tick methods.
    //
    // Iterate entities that carry both SensorResultsComponent and SensorBlackboardAdapter.
    auto view = mDomain.Query<SensorResultsComponent, SensorBlackboardAdapter>();
    for (auto entry : view)
    {
        const SensorResultsComponent& results = *std::get<0>(entry.components);
        SensorBlackboardAdapter*      adapter = std::get<1>(entry.components);

        adapter->Distil(results, adapter->GetBlackboard(), frameNumber);
        ++mLastAdapterRuns;
    }

    if (mAdaptersCounter)
        mAdaptersCounter->Inc(static_cast<uint64_t>(mLastAdapterRuns));
}

} // namespace Dia::Sensor
