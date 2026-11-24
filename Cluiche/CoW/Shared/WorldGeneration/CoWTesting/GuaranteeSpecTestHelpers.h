// Test utilities for the guarantee spec. Ships with the library (no gtest dependency): failures print and abort.
// Spec: docs/specs/features/draoi/guarantee-set-injection.md
#pragma once

#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorld/WorldStructure.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DraoiHexWorldTerrain/GuaranteeSpec.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace CoW {
namespace WorldGeneration {
namespace Testing {
using namespace Draoi::HexWorld;
using namespace Draoi::HexWorld::Testing;


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
    return Draoi::HexWorld::Testing::Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/guarantees.json", false);
}
inline std::string ShippedGuaranteesSidecarPath()
{
    return Draoi::HexWorld::Testing::Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/guarantees.schema.json", false);
}

inline std::string ShippedGuaranteesSidecarText() { return Draoi::HexWorld::Testing::Detail::ReadFileText(ShippedGuaranteesSidecarPath()); }

// The shipped guarantee spec, loaded once per process against the shipped zones. Aborts if it cannot be loaded.
inline const GuaranteeSpec& ShippedGuaranteeSpec()
{
    static GuaranteeSpec spec;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        GuaranteeSpecError err;
        const std::string text = Draoi::HexWorld::Testing::Detail::ReadFileText(ShippedGuaranteesPath());
        const std::string sidecar = Draoi::HexWorld::Testing::Detail::ReadFileText(ShippedGuaranteesSidecarPath());
        if (text.empty() || sidecar.empty() ||
            !LoadGuaranteeSpec(text.c_str(), sidecar.c_str(), Draoi::HexWorld::Testing::ShippedStratumTable(), spec, err))
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
