////////////////////////////////////////////////////////////////////////////////
// Filename: IPickable2D.h
// Description: Interface for any 2D object that can be picked by world position.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaPicking/PickLayer.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Geometry2D { class AARect; }
namespace Dia::Geometry2DPicking { struct PickHit2D; }

namespace Dia::Geometry2DPicking {

static constexpr unsigned int kMaxAreaHits = 64;

class IPickable2D
{
public:
    virtual ~IPickable2D() = default;

    virtual Dia::Core::StringCRC  GetPickableId() const = 0;
    virtual int                   GetPriority()   const = 0;
    virtual Dia::Picking::PickLayer GetLayer()    const = 0;

    // Point pick — returns true if worldPos hits this pickable, fills out.
    virtual bool TryPick(const Dia::Maths::Vector2D& worldPos, PickHit2D& out) const = 0;

    // Area pick — appends all hits within worldRect to out.
    virtual void PickArea(const Dia::Geometry2D::AARect& worldRect,
                          Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits>& out) const = 0;
};

} // namespace Dia::Geometry2DPicking
