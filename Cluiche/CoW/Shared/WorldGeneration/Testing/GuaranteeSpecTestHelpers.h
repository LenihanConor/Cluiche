// Test utilities for the guarantee spec. Ships with the library (no gtest dependency): failures print and abort.
// Spec: docs/specs/features/draoi/guarantee-set-injection.md
#pragma once

#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <Shared/WorldGeneration/GuaranteeSpec.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

// Kind ids of the shipped guarantees.json, for tests that build CoW worlds by hand (production code reads kinds from
// the GuaranteeSpec).
namespace SiteKinds
{
    inline Dia::Core::StringCRC Hearth()       { return Dia::Core::StringCRC("CoW.Site.Hearth"); }
    inline Dia::Core::StringCRC Nexus()        { return Dia::Core::StringCRC("CoW.Site.Nexus"); }
    inline Dia::Core::StringCRC DynastyStart() { return Dia::Core::StringCRC("CoW.Site.DynastyStart"); }
    inline Dia::Core::StringCRC WorldThrone()  { return Dia::Core::StringCRC("CoW.Site.WorldThrone"); }
}

// Absolute paths of the shipped Global/World/guarantees.json and its sidecar guarantees.schema.json.
inline std::string ShippedGuaranteesPath()
{
    return World::Testing::Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/guarantees.json", false);
}
inline std::string ShippedGuaranteesSidecarPath()
{
    return World::Testing::Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/guarantees.schema.json", false);
}

inline std::string ShippedGuaranteesSidecarText() { return World::Testing::Detail::ReadFileText(ShippedGuaranteesSidecarPath()); }

// The shipped guarantee spec, loaded once per process against the shipped zones. Aborts if it cannot be loaded.
inline const GuaranteeSpec& ShippedGuaranteeSpec()
{
    static GuaranteeSpec spec;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        GuaranteeSpecError err;
        const std::string text = World::Testing::Detail::ReadFileText(ShippedGuaranteesPath());
        const std::string sidecar = World::Testing::Detail::ReadFileText(ShippedGuaranteesSidecarPath());
        if (text.empty() || sidecar.empty() ||
            !LoadGuaranteeSpec(text.c_str(), sidecar.c_str(), World::Testing::ShippedZoneTable(), spec, err))
        {
            std::fprintf(stderr, "GuaranteeSpecTestHelpers: failed to load shipped guarantees: entry=%s reason=%s\n",
                         err.entry.c_str(), err.reason.c_str());
            std::fflush(stderr);
            std::abort();
        }
    }
    return spec;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
