// S-1 Reachability Validator: result and evidence helpers. See ValidationTypes.h.
#include <Shared/WorldGeneration/ValidationTypes.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

using Dia::Core::StringCRC;

bool Diagnostic::operator==(const Diagnostic& o) const { return std::strncmp(text, o.text, sizeof(text)) == 0; }

namespace MeasurementKeys
{
    StringCRC PairDistance()          { return StringCRC("pairDistance"); }
    StringCRC MinimumSpacing()        { return StringCRC("minimumSpacing"); }
    StringCRC ViolationCount()        { return StringCRC("violationCount"); }
}

void GuaranteeResult::AddDiagnostic(const char* format, ...)
{
    if (diagnostics.Size() >= kMaxDiagnostics)
    {
        ++droppedDiagnostics;
        return;
    }
    Diagnostic d;
    va_list args;
    va_start(args, format);
    std::vsnprintf(d.text, sizeof(d.text), format, args);
    va_end(args);
    d.text[sizeof(d.text) - 1] = '\0';
    diagnostics.Add(d);
}

const GuaranteeResult* ValidationResult::Find(uint32_t idCrc) const
{
    for (const GuaranteeResult& g : guarantees)
    {
        if (g.idCrc == idCrc) { return &g; }
    }
    return nullptr;
}

bool ValidationParams::AddProfile(const INavigationProfile* profile)
{
    if (!profile || profiles.IsFull()) { return false; }
    profiles.Add(profile);
    return true;
}

bool ValidationParams::AddGuarantee(const IWorldGuarantee* guarantee)
{
    if (!guarantee || guarantees.IsFull()) { return false; }
    guarantees.Add(guarantee);
    return true;
}

} // namespace WorldGeneration
} // namespace CoW
