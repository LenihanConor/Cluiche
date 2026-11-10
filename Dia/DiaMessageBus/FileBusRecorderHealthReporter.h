#pragma once

#ifdef DIA_DEBUG

#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::MessageBus { class FileBusRecorder; }

namespace Dia::MessageBus {

    // -----------------------------------------------------------------------
    // FileBusRecorderHealthReporter  (Debug-only)
    //
    // Reports Failing while the watched FileBusRecorder is disabled (open or
    // write failure), OK otherwise.
    //
    // NOT wired into MessageBusModule: the APPLICATION that owns the recorder
    // owns this reporter, registers it with HealthRegistry, and calls Check()
    // (e.g. once per tick). The recorder must outlive the reporter.
    // -----------------------------------------------------------------------
    class FileBusRecorderHealthReporter : public Dia::Observation::Health::HealthReporterBase
    {
    public:
        FileBusRecorderHealthReporter(Dia::Core::StringCRC reporterName, const FileBusRecorder& recorder);
        ~FileBusRecorderHealthReporter() override = default;

        Dia::Core::StringCRC GetReporterName() const override;

        // Re-evaluates recorder state; sets Failing when disabled, OK otherwise.
        void Check();

    private:
        Dia::Core::StringCRC     mName;
        const FileBusRecorder&   mRecorder;
    };

} // namespace Dia::MessageBus

#endif // DIA_DEBUG
