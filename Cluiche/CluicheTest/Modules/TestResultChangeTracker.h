#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "Modules/TestStages/TestResultsRegistry.h"
#include "Types/StageResult.h"

#include <cstdint>

namespace CluicheTest {

// Pure mapping + change detection from TestResultsRegistry states to StageCatalogue StageResultEvents.
// No module machinery: the registry is read, events are written to a caller buffer.
class TestResultChangeTracker
{
public:
    static constexpr unsigned int kMaxTracked = 32;

    // Maps a registry state to an event result (severity, label, utcMs). False for kNotRun (nothing to report).
    static bool MapState(StageResult::State state, uint64_t utcMs, Cluiche::AppFlow::StageResult& out);

    // Writes one event per stage whose state differs from the last one reported, up to maxEvents.
    // Stages left over because the buffer was full are picked up by the next call. Returns the events written.
    unsigned int Poll(const TestResultsRegistry& registry, uint64_t utcMs,
                      Cluiche::AppFlow::StageResultEvent* outEvents, unsigned int maxEvents);

private:
    struct Tracked
    {
        Dia::Core::StringCRC stage;
        StageResult::State state = StageResult::State::kNotRun;
    };

    Dia::Core::Containers::DynamicArrayC<Tracked, kMaxTracked> mTracked;
};

} // namespace CluicheTest
