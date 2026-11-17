#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: PerfWindowCommands.h
//
// Perf Inspector (task 2b): Main-hosted DiaAPI commands perf.window and perf.frame.
// Owns the Main-side CoarseChunkAssembler (60 s CoarseRing per processing unit) and a
// reader on the framework "$perfcoarse" EventStream.  Drain() keeps the rings current
// every Main tick (cheap when nothing is pending); packing happens only when a command
// is called, so with no client attached nothing is packed.
//
// Registered in ALL builds (the coarse stream and ring exist in Release and the
// Inspector must work against Release).  Do NOT gate on _DEBUG.
//
// Main thread only.  Does not own the store or the inspectable; both must outlive it.
////////////////////////////////////////////////////////////////////////////////

#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/JobChunk.h>
#include <DiaObservation/Profile/JobChunkAssembler.h>
#include <DiaStreams/EventStreamStore.h>

#include <cstdint>
#include <memory>

namespace Cluiche { namespace AppFlow {

class PerfWindowCommands
{
public:
    // store: the "$perfcoarse" store (may be null: Drain is then a no-op).  A reader is registered on it.
    // app:   optional, used only to map unit/module crcs to display names (null: hex names).
    // jobsStore: the "$perfjobs" store (may be null: no jobs data).  A reader is registered on it.
    // Task 13 part B: the same Drain() also feeds a JobChunkAssembler; perf.window gains an added
    // "jobs" section (tier 2 aggregates + empty "steps") and perf.jobs {unit, fromNs, toNs} serves
    // raw job records (tier 3, at most kMaxJobRecords per reply).
    PerfWindowCommands(Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::CoarseChunk>* store,
                       Dia::ApplicationFlow::IApplicationInspectable* app,
                       Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::JobChunk>* jobsStore = nullptr);

    static constexpr uint32_t kMaxJobRecords = 2000;   // perf.jobs reply cap (AC13)
    ~PerfWindowCommands();

    PerfWindowCommands(const PerfWindowCommands&) = delete;
    PerfWindowCommands& operator=(const PerfWindowCommands&) = delete;

    // Registers perf.window and perf.frame.  Idempotent.
    void Register();
    // Removes them again.  Idempotent.
    void Unregister();

    // Reads every pending chunk from the stream and applies it to the assembler.
    void Drain();

    // perf.window {seconds?}: packed window of every known unit.  Never fails (no units -> empty list).
    Json::Value HandleWindow(const Json::Value& args);
    // perf.frame {unit, frame}: failures return {"error": reason}.
    Json::Value HandleFrame(const Json::Value& args);
    // perf.jobs {unit, fromNs, toNs}: failures return {"error": reason}.
    Json::Value HandleJobs(const Json::Value& args);

    const Dia::Observation::Profile::CoarseChunkAssembler& GetAssembler() const { return mAssembler; }
    const Dia::Observation::Profile::JobChunkAssembler&    GetJobsAssembler() const { return mJobsAssembler; }

private:
    Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::CoarseChunk>* mStore;
    Dia::ApplicationFlow::IApplicationInspectable*                                  mApp;
    int                                                                             mReader;
    Dia::Observation::Profile::CoarseChunkAssembler                                 mAssembler;
    Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::JobChunk>*    mJobsStore;
    int                                                                             mJobsReader;
    Dia::Observation::Profile::JobChunkAssembler                                    mJobsAssembler;
    bool                                                                            mRegistered;
    // Drain scratch (4 x ~8.6 KB events): heap, so Drain never puts it on the stack.
    typedef Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::Observation::Profile::CoarseChunk>, 4> DrainBuffer;
    std::unique_ptr<DrainBuffer>                                                    mScratch;
    typedef Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::Observation::Profile::JobChunk>, 4> JobDrainBuffer;
    std::unique_ptr<JobDrainBuffer>                                                 mJobsScratch;
};

}} // namespace Cluiche::AppFlow
