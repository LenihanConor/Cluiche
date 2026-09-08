#include <DiaSensor/SoundEventList.h>

namespace Dia::Sensor {

void SoundEventList::Emit(const Dia::Maths::Vector2D& position, SoundType type, float radius, int frameNumber)
{
    if (mEvents.IsFull())
    {
        return;
    }

    SoundEmission emission;
    emission.event.position  = position;
    emission.event.type      = type;
    emission.event.timestamp = frameNumber;
    emission.emissionRadius  = radius;
    mEvents.Add(emission);
}

void SoundEventList::Clear()
{
    mEvents.RemoveAll();
}

const Dia::Core::Containers::DynamicArrayC<SoundEmission, kMaxSoundEventsPerFrame>& SoundEventList::GetEvents() const
{
    return mEvents;
}

} // namespace Dia::Sensor
