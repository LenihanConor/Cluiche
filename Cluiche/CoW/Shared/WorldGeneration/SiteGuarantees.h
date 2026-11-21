// Transitional CoW site-kind accessors. The guarantee rules are generic now (SiteRules.h, driven by guarantees.json);
// these remain only for WorldGenSession, WorldOverlayGeometry and WorldGenViewHost until generic-special-sites tasks 3-4.
#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace CoW {
namespace WorldGeneration {

namespace SiteKinds
{
    Dia::Core::StringCRC Hearth();         // "CoW.Site.Hearth"
    Dia::Core::StringCRC Nexus();          // "CoW.Site.Nexus"
    Dia::Core::StringCRC DynastyStart();   // "CoW.Site.DynastyStart"
    Dia::Core::StringCRC WorldThrone();    // "CoW.Site.WorldThrone"
}

constexpr float kDefaultMinimumDynastySpacing = 10.0f;   // hex steps; placeholder (GDD open question 1)

} // namespace WorldGeneration
} // namespace CoW
