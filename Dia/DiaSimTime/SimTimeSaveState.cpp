#include <DiaSimTime/SimTimeSaveState.h>

#include <DiaSimTime/SimTimeScheduler.h>
#include <DiaSimTime/SimTimeRegistry.h>
#include <DiaSimTime/SimTimeState.h>

#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>

#include <DiaObservation/Log/DiaLog.h>

namespace Dia::SimTime {

    using Dia::Core::StringCRC;
    using Dia::Core::TimeAbsolute;
    using Dia::Core::TimeRelative;

    namespace {

        // --- JSON keys (StringCRC's AsChar() is used as the jsoncpp member key) ---
        const StringCRC kKeyGameTime      ("gameTime");
        const StringCRC kKeyScheduler     ("scheduler");
        const StringCRC kKeySystems       ("systems");
        const StringCRC kKeyEventType     ("eventType");
        const StringCRC kKeyTargetSystemId("targetSystemId");
        const StringCRC kKeyFireTime      ("fireTime");
        const StringCRC kKeyRecurring     ("recurring");
        const StringCRC kKeyInterval      ("interval");
        const StringCRC kKeySystemId      ("systemId");
        const StringCRC kKeyState         ("state");

        // StringCRC identity is purely its 32-bit CRC value (operator== compares
        // only mCRC), so persisting eventType/targetSystemId as their CRC value —
        // per the spec's "each an int64_t" — and rebuilding a StringCRC with the
        // same value round-trips identity faithfully. The rebuilt StringCRC has an
        // empty debug string, which is irrelevant: every consumer (scheduler
        // payloads, registry FindEntry, wake-sentinel matching) compares by value.
        StringCRC CrcFromValue(int64_t value)
        {
            StringCRC crc;
            static_cast<Dia::Core::CRC&>(crc) = Dia::Core::CRC(static_cast<unsigned int>(value));
            return crc;
        }

    } // anonymous namespace

    SimTimeSaveState::SimTimeSaveState(SimTimeDomain&    worldDomain,
                                       SimTimeScheduler& scheduler,
                                       SimTimeRegistry&  registry)
        : mWorldDomain(worldDomain)
        , mScheduler(scheduler)
        , mRegistry(registry)
    {
    }

    // -------------------------------------------------------------------------
    // Serialize
    // -------------------------------------------------------------------------
    void SimTimeSaveState::Serialize(Dia::SaveGame::SaveContext& ctx) const
    {
        // 1. World game clock.
        ctx.Write(kKeyGameTime, mWorldDomain.Now().AsLongLongInMicroseconds());

        // 2. Scheduler pending entries. Recurrence is persisted (recurring +
        //    interval) so a recurring entry keeps re-arming after restore; a
        //    one-shot stores recurring=false and interval is ignored on load.
        Dia::Core::Containers::DynamicArrayC<
            SimTimeScheduler::PendingEntryView, SimTimeScheduler::kMaxEntries> pending;
        mScheduler.GetPendingEntries(pending);

        ctx.BeginArray(kKeyScheduler);
        for (unsigned int i = 0; i < pending.Size(); ++i)
        {
            const SimTimeScheduler::PendingEntryView& e = pending[i];
            Json::Value elem(Json::objectValue);
            elem[kKeyEventType.AsChar()]      = static_cast<Json::Int64>(e.eventType.Value());
            elem[kKeyTargetSystemId.AsChar()] = static_cast<Json::Int64>(e.targetSystemId.Value());
            elem[kKeyFireTime.AsChar()]       = static_cast<Json::Int64>(e.fireTime.AsLongLongInMicroseconds());
            elem[kKeyRecurring.AsChar()]      = e.recurring;
            elem[kKeyInterval.AsChar()]       = static_cast<Json::Int64>(e.recurringInterval.AsLongLongInMicroseconds());
            ctx.CurrentNode().append(elem);
        }
        ctx.EndArray();

        // 3. Per-system sleep state, keyed by systemId.
        ctx.BeginArray(kKeySystems);
        const int count = mRegistry.GetRegisteredCount();
        for (int i = 0; i < count; ++i)
        {
            const SimTimeRegistryEntryView view = mRegistry.GetEntryAt(i);
            const SimTimeState             st   = mRegistry.GetState(view.systemId);
            Json::Value elem(Json::objectValue);
            elem[kKeySystemId.AsChar()] = static_cast<Json::Int64>(view.systemId.Value());
            elem[kKeyState.AsChar()]    = static_cast<Json::Int>(
                                              st == SimTimeState::kSleeping ? 1 : 0);
            ctx.CurrentNode().append(elem);
        }
        ctx.EndArray();
    }

    // -------------------------------------------------------------------------
    // Deserialize — fixed order (ST-015): domain -> scheduler -> sleep state.
    // -------------------------------------------------------------------------
    void SimTimeSaveState::Deserialize(Dia::SaveGame::LoadContext& ctx)
    {
        // 1. Re-anchor the world clock. A freshly-constructed domain starts at its
        //    minimum time, so AdvanceTo() is a genuine forward jump.
        int64_t gameTimeUs = 0;
        if (ctx.Read(kKeyGameTime, gameTimeUs))
        {
            mWorldDomain.AdvanceTo(TimeAbsolute::CreateFromMicroseconds(
                                       static_cast<long long>(gameTimeUs)));
        }

        // 2. Re-insert scheduler entries under FRESH handles (ST-016). One-shots
        //    via ScheduleAt(savedTime); recurring via ScheduleRecurring(interval)
        //    then Reschedule() to the saved absolute next-fire time — this keeps
        //    BOTH the exact next fire time AND the recurring interval, so the
        //    entry re-arms at the same phase after restore.
        uint32_t schedCount = 0;
        if (ctx.BeginArray(kKeyScheduler, schedCount))
        {
            for (uint32_t i = 0; i < schedCount; ++i)
            {
                ctx.SetArrayIndex(i);
                int64_t eventTypeVal = 0;
                int64_t targetVal    = 0;
                int64_t fireUs       = 0;
                bool    recurring    = false;
                int64_t intervalUs   = 0;
                ctx.Read(kKeyEventType,      eventTypeVal);
                ctx.Read(kKeyTargetSystemId, targetVal);
                ctx.Read(kKeyFireTime,       fireUs);
                ctx.Read(kKeyRecurring,      recurring);
                ctx.Read(kKeyInterval,       intervalUs);

                const StringCRC     eventType = CrcFromValue(eventTypeVal);
                const StringCRC     target    = CrcFromValue(targetVal);
                const TimeAbsolute  fireTime  = TimeAbsolute::CreateFromMicroseconds(
                                                    static_cast<long long>(fireUs));

                if (recurring)
                {
                    // Intervals are ms-range (well under float's exact-integer
                    // range in microseconds), so float reconstruction is exact.
                    const TimeRelative interval = TimeRelative::CreateFromMicroseconds(
                                                      static_cast<float>(intervalUs));
                    const ScheduleHandle h = mScheduler.ScheduleRecurring(interval, eventType, target);
                    mScheduler.Reschedule(h, fireTime);
                }
                else
                {
                    mScheduler.ScheduleAt(fireTime, eventType, target);
                }
            }
            ctx.EndArray();
        }

        // 3. Re-apply sleep state for systemIds STILL registered; log-and-skip any
        //    that are not (content may have changed between the save and now).
        uint32_t sysCount = 0;
        if (ctx.BeginArray(kKeySystems, sysCount))
        {
            for (uint32_t i = 0; i < sysCount; ++i)
            {
                ctx.SetArrayIndex(i);
                int64_t systemIdVal = 0;
                int32_t stateVal    = 0;
                ctx.Read(kKeySystemId, systemIdVal);
                ctx.Read(kKeyState,    stateVal);

                const StringCRC systemId = CrcFromValue(systemIdVal);
                if (!mRegistry.IsRegistered(systemId))
                {
                    DIA_LOG_WARNING("SimTime",
                        "SimTimeSaveState: saved systemId 0x%08X is not registered this session — skipping its sleep state",
                        static_cast<unsigned int>(systemIdVal));
                    continue;
                }
                if (stateVal == 1)
                {
                    mRegistry.Sleep(systemId);
                }
                else
                {
                    mRegistry.Wake(systemId);
                }
            }
            ctx.EndArray();
        }
    }

} // namespace Dia::SimTime
