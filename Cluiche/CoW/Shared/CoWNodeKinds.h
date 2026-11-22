// CoW's ids for the four built-in node kinds the world generator places (NodeKindIds). These are CoW content and saved
// ids: they appear in world files and goldens, so changing them breaks every saved CoW world.
#pragma once

#include <Shared/WorldGeneration/HexWorldAccess.h>
#include <DraoiHexWorld/Sites.h>
#include <Shared/WorldGeneration/SiteRegistry.h>

namespace CoW {
namespace WorldGeneration {

inline NodeKindIds CoWNodeKindIds()
{
    NodeKindIds ids;
    ids.corridor = Dia::Core::StringCRC("CoW.Node.Corridor");
    ids.river    = Dia::Core::StringCRC("CoW.Node.River");
    ids.hunting  = Dia::Core::StringCRC("CoW.Node.Hunting");
    ids.resource = Dia::Core::StringCRC("CoW.Node.Resource");
    return ids;
}

// A SiteRegistry that carries CoW's node kind ids (what a CoW host constructs).
class CoWSiteRegistry : public SiteRegistry
{
public:
    CoWSiteRegistry() { SetNodeKinds(CoWNodeKindIds()); }
};

// The CoW kinds by name (call sites that name one kind).
namespace BuiltinNodeKinds
{
    inline Dia::Core::StringCRC Corridor() { return CoWNodeKindIds().corridor; }
    inline Dia::Core::StringCRC River()    { return CoWNodeKindIds().river; }
    inline Dia::Core::StringCRC Hunting()  { return CoWNodeKindIds().hunting; }
    inline Dia::Core::StringCRC Resource() { return CoWNodeKindIds().resource; }
}

} // namespace WorldGeneration
} // namespace CoW
