////////////////////////////////////////////////////////////////////////////////
// Filename: DiaCapture.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaObservation/Session/SessionManager.h>
#include <DiaObservation/Capture/CaptureManager.h>
#include <DiaObservation/Capture/CaptureTypes.h>
#include <DiaCore/CRC/StringCRC.h>

// Request a screen capture from the active session.
// tag     - StringCRC or string literal identifying this capture point
// context - short free-form string describing the reason (e.g. "passed", "failed")
// No-ops silently if no session is active or the capture ring is full.
#define DIA_CAPTURE(dia_tag_, dia_ctx_)                                                  \
    do {                                                                                 \
        auto* _cm = ::Dia::Observation::SessionManager::GetActiveCaptureManager();      \
        if (_cm)                                                                         \
        {                                                                                \
            ::Dia::Observation::Capture::CaptureMetadata _meta;                         \
            _meta.tag     = ::Dia::Core::StringCRC(dia_tag_);                           \
            _meta.trigger = ::Dia::Observation::Capture::TriggerSource::kCode;          \
            strncpy_s(_meta.context, sizeof(_meta.context), (dia_ctx_), _TRUNCATE);      \
            _cm->RequestCapture(_meta);                                                  \
        }                                                                                \
    } while (0)
