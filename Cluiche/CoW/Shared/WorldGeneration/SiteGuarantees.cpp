// Transitional CoW site-kind accessors. See SiteGuarantees.h.
#include <Shared/WorldGeneration/SiteGuarantees.h>

namespace CoW {
namespace WorldGeneration {

using Dia::Core::StringCRC;

namespace SiteKinds
{
    StringCRC Hearth()       { return StringCRC("CoW.Site.Hearth"); }
    StringCRC Nexus()        { return StringCRC("CoW.Site.Nexus"); }
    StringCRC DynastyStart() { return StringCRC("CoW.Site.DynastyStart"); }
    StringCRC WorldThrone()  { return StringCRC("CoW.Site.WorldThrone"); }
}

} // namespace WorldGeneration
} // namespace CoW
