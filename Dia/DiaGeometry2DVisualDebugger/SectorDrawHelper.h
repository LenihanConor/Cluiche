////////////////////////////////////////////////////////////////////////////////
// Filename: SectorDrawHelper.h
// Description: Tessellates a Geometry2D Sector into a ConvexPolygon for rendering.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

namespace Dia { namespace Geometry2D { class Sector; class ConvexPolygon; } }

namespace Dia { namespace Geometry2DVisualDebugger {

// Converts a Sector (pie-slice) into a ConvexPolygon fan: centre vertex first,
// then 'arcSegments' vertices along the arc, closing back toward the centre edge.
// Total vertex count = arcSegments + 2 (must not exceed ConvexPolygon::kMaxVertices = 16).
// 'arcSegments' is clamped so that arcSegments + 2 <= 16.
Dia::Geometry2D::ConvexPolygon SectorToConvexPolygon(const Dia::Geometry2D::Sector& sector, int arcSegments = 8);

} } // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
