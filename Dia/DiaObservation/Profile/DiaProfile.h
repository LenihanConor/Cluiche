#pragma once

#include <DiaObservation/Profile/ScopedZone.h>
#include <DiaCore/CRC/StringCRC.h>

#define DIA_PROFILE_SCOPE(name, category) \
    ::Dia::Observation::Profile::ScopedZone _dia_profile_##__LINE__( \
        ::Dia::Core::StringCRC(name), (category))

#define DIA_PROFILE_SCOPE_NAMED(var, name, category) \
    ::Dia::Observation::Profile::ScopedZone var(::Dia::Core::StringCRC(name), (category))

#define DIA_PROFILE_SCOPE_METRIC(name, category, histogram) \
    ::Dia::Observation::Profile::ScopedZone _dia_profile_##__LINE__( \
        ::Dia::Core::StringCRC(name), (category), (histogram))
