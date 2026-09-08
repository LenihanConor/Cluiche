#pragma once

#include <DiaObservation/Profile/ScopedZone.h>
#include <DiaCore/CRC/StringCRC.h>

// Double-expansion macros so __LINE__ expands to its numeric value before pasting.
// Required on MSVC /Zc:preprocessor and other conforming preprocessors.
#define DIA_CONCAT_(a, b) a##b
#define DIA_CONCAT(a, b)  DIA_CONCAT_(a, b)

#define DIA_PROFILE_SCOPE(name, category) \
    ::Dia::Observation::Profile::ScopedZone DIA_CONCAT(_dia_profile_, __LINE__)( \
        ::Dia::Core::StringCRC(name), (category))

#define DIA_PROFILE_SCOPE_NAMED(var, name, category) \
    ::Dia::Observation::Profile::ScopedZone var(::Dia::Core::StringCRC(name), (category))

#define DIA_PROFILE_SCOPE_METRIC(name, category, histogram) \
    ::Dia::Observation::Profile::ScopedZone DIA_CONCAT(_dia_profile_, __LINE__)( \
        ::Dia::Core::StringCRC(name), (category), (histogram))
