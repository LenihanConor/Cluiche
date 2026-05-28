////////////////////////////////////////////////////////////////////////////////
// Filename: CapsuleDrawHelper.h
// Description: Tessellates a Geometry2D Capsule into a ConvexPolygon for rendering.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

namespace Dia { namespace Geometry2D { class Capsule; class ConvexPolygon; } }

namespace Dia { namespace Geometry2DVisualDebugger {

// Converts a Capsule (two endpoint circles joined by tangent lines) into a
// ConvexPolygon approximation.  The outline is built as two semicircular end
// caps stitched into a single vertex loop (12 vertices total: 6 per cap).
Dia::Geometry2D::ConvexPolygon CapsuleToConvexPolygon(const Dia::Geometry2D::Capsule& capsule);

} } // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
