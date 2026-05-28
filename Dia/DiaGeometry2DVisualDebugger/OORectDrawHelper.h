////////////////////////////////////////////////////////////////////////////////
// Filename: OORectDrawHelper.h
// Description: Converts a Geometry2D OORect into a ConvexPolygon for rendering.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

namespace Dia { namespace Geometry2D { class OORect; class ConvexPolygon; } }

namespace Dia { namespace Geometry2DVisualDebugger {

// Converts an OORect (4 explicit corner points) directly into a ConvexPolygon
// with 4 vertices, preserving the original winding order.
Dia::Geometry2D::ConvexPolygon OORectToConvexPolygon(const Dia::Geometry2D::OORect& rect);

} } // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
