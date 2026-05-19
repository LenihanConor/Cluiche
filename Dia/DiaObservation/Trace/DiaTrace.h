#pragma once

#include <DiaObservation/Trace/ScopedZone.h>
#include <DiaCore/CRC/StringCRC.h>

#define DIA_TRACE_ZONE_CONCAT2(a, b) a##b
#define DIA_TRACE_ZONE_CONCAT(a, b) DIA_TRACE_ZONE_CONCAT2(a, b)

#define DIA_TRACE_ZONE(name) \
	::Dia::Observation::Trace::ScopedZone DIA_TRACE_ZONE_CONCAT(_dia_zone_, __LINE__)(::Dia::Core::StringCRC(name))

#define DIA_TRACE_ZONE_NAMED(var, name) \
	::Dia::Observation::Trace::ScopedZone var(::Dia::Core::StringCRC(name))
