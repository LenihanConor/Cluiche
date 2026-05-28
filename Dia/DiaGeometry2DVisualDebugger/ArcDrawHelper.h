////////////////////////////////////////////////////////////////////////////////
// Filename: ArcDrawHelper.h
// Description: Tessellates a Geometry2D Arc into a ConvexPolygon for rendering.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

namespace Dia { namespace Geometry2D { class Arc; class ConvexPolygon; } }

namespace Dia { namespace Geometry2DVisualDebugger {

// Converts an Arc (centre + axis + half-angle + radius) into a ConvexPolygon
// by sampling the arc curve at evenly-spaced angular steps.
// 'segments' must be in [2, 16].
Dia::Geometry2D::ConvexPolygon ArcToConvexPolygon(const Dia::Geometry2D::Arc& arc, int segments = 8);

} } // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
