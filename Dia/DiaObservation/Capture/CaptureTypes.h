////////////////////////////////////////////////////////////////////////////////
// Filename: CaptureTypes.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace Observation
    {
        namespace Capture
        {
            enum class TriggerSource : unsigned char
            {
                kManual,        // Human-initiated (hotkey, debug menu)
                kAutomation,    // Orchestrator / DiaRemoteControl command
                kCode           // Programmatic (checkpoint, test logic)
            };

            struct CaptureMetadata
            {
                Dia::Core::StringCRC tag;           // e.g. StringCRC("checkpoint_settle")
                TriggerSource        trigger = TriggerSource::kCode;
                char                 context[128] = {};  // Free-form context string
            };

            enum class CaptureRequestStatus : unsigned char
            {
                kAccepted,              // Request queued for capture
                kRejected_RingFull,     // ICanvas ring buffer full
                kRejected_NoCanvas,     // No ICanvas configured
                kRejected_NoSession     // SessionManager not started
            };
        }
    }
}
