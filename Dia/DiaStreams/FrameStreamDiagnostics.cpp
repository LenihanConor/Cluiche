#include <DiaStreams/FrameStreamDiagnostics.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Trace/TraceCategory.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace ApplicationFlow {

void FrameStream_WarnNoData(const Dia::Core::StringCRC& id)
{
    DIA_TRACE_ZONE("FrameStream.AutoFlush.NoData", Dia::Observation::Trace::Category::kDiaApplicationFlow);
    DIA_LOG_WARNING("DiaApplicationFlow",
        "FrameStream '%s' has no data — no writer has published to this stream. "
        "Check that a writer module is active for the current stage.",
        id.AsChar());
}

}} // namespace Dia::ApplicationFlow
