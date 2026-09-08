#pragma once
#ifndef DIA_SENSOR_SOUNDEVENTLIST_H
#define DIA_SENSOR_SOUNDEVENTLIST_H

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaSensor/SensorResults.h>

namespace Dia::Sensor {

struct SoundEmission {
    SoundEvent event;
    float      emissionRadius = 0.f;  // entities within this range hear it
};

static constexpr unsigned int kMaxSoundEventsPerFrame = 64;

class SoundEventList {
public:
    void Emit(const Dia::Maths::Vector2D& position, SoundType type, float radius, int frameNumber);
    void Clear();
    const Dia::Core::Containers::DynamicArrayC<SoundEmission, kMaxSoundEventsPerFrame>& GetEvents() const;

private:
    Dia::Core::Containers::DynamicArrayC<SoundEmission, kMaxSoundEventsPerFrame> mEvents;
};

} // namespace Dia::Sensor
#endif // DIA_SENSOR_SOUNDEVENTLIST_H
