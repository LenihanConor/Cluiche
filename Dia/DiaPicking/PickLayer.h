////////////////////////////////////////////////////////////////////////////////
// Filename: PickLayer.h
// Description: Layer bitmask for filtering pickable objects by category.
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia::Picking {

enum class PickLayer : unsigned int
{
    kDefault  = 1 << 0,
    kUnit     = 1 << 1,
    kTerrain  = 1 << 2,
    kUI       = 1 << 3,
    kDebug    = 1 << 4,
    kAll      = 0xFFFFFFFF
};

using PickLayerMask = unsigned int;

inline PickLayerMask LayerBit(PickLayer layer)
{
    return static_cast<PickLayerMask>(layer);
}

inline bool LayerMatches(PickLayer layer, PickLayerMask mask)
{
    return (static_cast<PickLayerMask>(layer) & mask) != 0;
}

inline PickLayerMask operator|(PickLayer a, PickLayer b)
{
    return static_cast<PickLayerMask>(a) | static_cast<PickLayerMask>(b);
}

inline PickLayerMask operator|(PickLayerMask a, PickLayer b)
{
    return a | static_cast<PickLayerMask>(b);
}

} // namespace Dia::Picking
