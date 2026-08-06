#pragma once

#include <DiaObjective/IObjectiveObserver.h>
#include <DiaObjective/ObjectiveDef.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Objective { namespace Testing {

    class CapturingObserver : public IObjectiveObserver
    {
    public:
        struct Event
        {
            enum class Type { Activated, Completed, Failed };
            Type                 type;
            Dia::Core::StringCRC id;
        };

        const Dia::Core::Containers::DynamicArrayC<Event, 32>& GetEvents() const { return mEvents; }
        void Clear() { mEvents.RemoveAll(); }

        void OnObjectiveActivated(Dia::Core::StringCRC id) override
        {
            if (!mEvents.IsFull())
            {
                Event e;
                e.type = Event::Type::Activated;
                e.id   = id;
                mEvents.Add(e);
            }
        }

        void OnObjectiveCompleted(Dia::Core::StringCRC id, const RewardPayload&) override
        {
            if (!mEvents.IsFull())
            {
                Event e;
                e.type = Event::Type::Completed;
                e.id   = id;
                mEvents.Add(e);
            }
        }

        void OnObjectiveFailed(Dia::Core::StringCRC id) override
        {
            if (!mEvents.IsFull())
            {
                Event e;
                e.type = Event::Type::Failed;
                e.id   = id;
                mEvents.Add(e);
            }
        }

    private:
        Dia::Core::Containers::DynamicArrayC<Event, 32> mEvents;
    };

}}} // namespace Dia::Objective::Testing
