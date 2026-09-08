#include "DiaGeometry3D/Shapes/Ray.h"
#include "DiaCore/Core/Assert.h"

namespace Dia::Geometry3D
{
    Ray::Ray(const Dia::Maths::Vector3D& origin, const Dia::Maths::Vector3D& direction)
        : mOrigin(origin)
        , mDirection(direction)
    {
        DIA_ASSERT(direction.SquareMagnitude() > 1e-10f, "Ray direction must be non-zero");
    }
}
