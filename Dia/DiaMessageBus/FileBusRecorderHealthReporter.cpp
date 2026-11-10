#ifdef DIA_DEBUG

#include <DiaMessageBus/FileBusRecorderHealthReporter.h>
#include <DiaMessageBus/FileBusRecorder.h>

namespace Dia::MessageBus {

FileBusRecorderHealthReporter::FileBusRecorderHealthReporter(Dia::Core::StringCRC reporterName,
                                                             const FileBusRecorder& recorder)
    : mName(reporterName)
    , mRecorder(recorder)
{
    SetOK();
}

Dia::Core::StringCRC FileBusRecorderHealthReporter::GetReporterName() const
{
    return mName;
}

void FileBusRecorderHealthReporter::Check()
{
    if (mRecorder.IsDisabled())
        SetFailing(Dia::Core::StringCRC("recorder_disabled"));
    else
        SetOK();
}

} // namespace Dia::MessageBus

#endif // DIA_DEBUG
