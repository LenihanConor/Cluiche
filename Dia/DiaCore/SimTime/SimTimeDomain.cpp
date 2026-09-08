#include <DiaCore/SimTime/SimTimeDomain.h>

namespace Dia::SimTime {

    SimTimeDomain::SimTimeDomain(Core::StringCRC id, float hz, Core::TimeAbsolute startTime)
        : mId(id)
        , mTimeServer(hz, startTime)
    {
    }

    Core::TimeAbsolute SimTimeDomain::Now() const
    {
        return mTimeServer.GetTime();
    }

    Core::TimeRelative SimTimeDomain::Step() const
    {
        return mTimeServer.GetStep();
    }

    void SimTimeDomain::SetScale(float scale)
    {
        mTimeServer.SetTimeScale(scale);
    }

    void SimTimeDomain::Pause()
    {
        mTimeServer.Pause();
    }

    void SimTimeDomain::Resume()
    {
        mTimeServer.Resume();
    }

    void SimTimeDomain::Step(Core::TimeRelative step)
    {
        mTimeServer.Step(step);
    }

    void SimTimeDomain::AdvanceTo(Core::TimeAbsolute t)
    {
        mTimeServer.AdvanceTo(t);
    }

    float SimTimeDomain::GetScale() const
    {
        return mTimeServer.GetTimeScale();
    }

    bool SimTimeDomain::IsPaused() const
    {
        return mTimeServer.IsPaused();
    }

    Core::StringCRC SimTimeDomain::GetId() const
    {
        return mId;
    }

    uint64_t SimTimeDomain::GetTick() const
    {
        return static_cast<uint64_t>(mTimeServer.GetTick());
    }

    void SimTimeDomain::Tick()
    {
        mTimeServer.Tick();
    }

} // namespace Dia::SimTime
