////////////////////////////////////////////////////////////////////////////////
// Filename: PickHit2D.h
// Description: A single 2D pick hit. Kind + named union for debuggability.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaPicking/PickLayer.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Geometry2DPicking {

struct PickHit2D
{
    enum class Kind : unsigned int
    {
        kHexCell     = 0,
        kSpatialCell = 1,
        kObject      = 2,
        kEntity      = 3
    };

    Dia::Core::StringCRC pickableId;
    Kind                 kind       = Kind::kObject;
    int                  priority   = 0;
    Dia::Maths::Vector2D worldPos;

    union
    {
        Dia::Geometry2D::HexCoord hexCell;
        struct { int x; int y; }  spatialCell;
        unsigned int              objectIdx;
        unsigned int              entityId;
    };

    PickHit2D() : hexCell{0, 0} {}
};

} // namespace Dia::Geometry2DPicking
