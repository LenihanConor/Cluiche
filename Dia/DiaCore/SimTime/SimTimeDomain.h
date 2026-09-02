#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaCore/Time/TimeServer.h>
#include <cstdint>

namespace Dia::SimTime {

    // A named, independently-pausable/scalable clock. Composes Dia::Core::TimeServer
    // rather than reimplementing clock logic — TimeServer's Pause/Resume/Step/AdvanceTo
    // API (added specifically to support this class) already does exactly what's needed.
    // The world SimTimeDomain is owned directly by the SimPU's ProcessingUnit as its
    // clock; named sub-domains (a later task, SimTimeDomainRegistry) wrap additional
    // instances of this same class for independent pause/slow-mo/fast-forward subtrees.
    class SimTimeDomain
    {
    public:
        SimTimeDomain(Core::StringCRC id, float hz, Core::TimeAbsolute startTime = Core::TimeAbsolute::Zero());

        Core::TimeAbsolute  Now() const;
        Core::TimeRelative  Step() const;       // current effective step size

        void  SetScale(float scale);
        void  Pause();
        void  Resume();
        void  Step(Core::TimeRelative step);    // manual single-step advance (bypasses pause)
        void  AdvanceTo(Core::TimeAbsolute t);

        float               GetScale() const;
        bool                IsPaused() const;
        Core::StringCRC     GetId() const;
        uint64_t            GetTick() const;

        void  Tick();  // called once per drained fixed-timestep by the owning ProcessingUnit's
                       // accumulator (a later task). Advances by (step size * current scale).

    private:
        Core::StringCRC     mId;
        Core::TimeServer    mTimeServer;
    };

} // namespace Dia::SimTime
