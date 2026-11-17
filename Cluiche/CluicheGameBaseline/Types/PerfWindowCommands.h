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
#include <DiaStreams/EventStreamStore.h>

#include <cstdint>
#include <memory>

namespace Cluiche { namespace AppFlow {

class PerfWindowCommands
{
public:
    // store: the "$perfcoarse" store (may be null: Drain is then a no-op).  A reader is registered on it.
    // app:   optional, used only to map unit/module crcs to display names (null: hex names).
    PerfWindowCommands(Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::CoarseChunk>* store,
                       Dia::ApplicationFlow::IApplicationInspectable* app);
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

    const Dia::Observation::Profile::CoarseChunkAssembler& GetAssembler() const { return mAssembler; }

private:
    Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::CoarseChunk>* mStore;
    Dia::ApplicationFlow::IApplicationInspectable*                                  mApp;
    int                                                                             mReader;
    Dia::Observation::Profile::CoarseChunkAssembler                                 mAssembler;
    bool                                                                            mRegistered;
    // Drain scratch (4 x ~8.6 KB events): heap, so Drain never puts it on the stack.
    typedef Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::Observation::Profile::CoarseChunk>, 4> DrainBuffer;
    std::unique_ptr<DrainBuffer>                                                    mScratch;
};

}} // namespace Cluiche::AppFlow
