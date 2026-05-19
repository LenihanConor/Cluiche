#pragma once

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <thread>
#include <mutex>

namespace Dia { namespace Observation { namespace Log {

// Thread-safe capture sink for tests. Records entries and the calling thread id.
struct MockSink : public ISink
{
    static const unsigned int kMaxEntries = 256;

    struct Record
    {
        LogEntry         entry;
        std::thread::id  callingThread;
    };

    Record           entries[kMaxEntries];
    unsigned int     count = 0;
    mutable std::mutex mutex;

    void OnLogEntry(const LogEntry& e) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (count < kMaxEntries)
        {
            entries[count].entry        = e;
            entries[count].callingThread = std::this_thread::get_id();
            ++count;
        }
    }

    const char* GetName() const override { return "MockSink"; }

    unsigned int Count() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return count;
    }

    std::thread::id CallingThreadAt(unsigned int i) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return entries[i].callingThread;
    }
};

} } } // namespace Dia::Observation::Log
