////////////////////////////////////////////////////////////////////////////////
// Filename: ViewportTransform.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGeometry2D/Shapes/AARect.h>

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		/// \brief Converts points between world space and screen (pixel) space
		///        for a 2D camera with Y-down screen coordinates.
		///
		/// Constructed from a Camera2D and the window size.  All methods
		/// are const — the transform is immutable after construction.
		///
		/// Coordinate conventions:
		///   - World origin can be anywhere; camera.GetPosition() is the
		///     world point that maps to the screen centre.
		///   - Zoom > 1 means the world appears larger on screen (zoomed in).
		///   - Rotation is clockwise in degrees.
		///   - Screen Y increases downward.
		////////////////////////////////////////////////////////////
		class ViewportTransform
		{
		public:
			////////////////////////////////////////////////////////////
			/// \brief Construct from a camera and the window dimensions.
			///
			/// \param camera      The 2D camera describing view centre, zoom, rotation.
			/// \param windowSize  Width (x) and height (y) of the render window in pixels.
			////////////////////////////////////////////////////////////
			ViewportTransform(const Camera2D& camera, const Dia::Maths::Vector2D& windowSize);

			////////////////////////////////////////////////////////////
			/// \brief Convert a screen pixel position to a world-space point.
			///
			/// \param pixel  Position in screen pixels (top-left origin, Y down).
			/// \return       Corresponding world-space position.
			////////////////////////////////////////////////////////////
			Dia::Maths::Vector2D ScreenToWorld(const Dia::Maths::Vector2D& pixel) const;

			////////////////////////////////////////////////////////////
			/// \brief Convert a world-space point to a screen pixel position.
			///
			/// \param world  World-space position.
			/// \return       Corresponding screen pixel position (top-left origin, Y down).
			////////////////////////////////////////////////////////////
			Dia::Maths::Vector2D WorldToScreen(const Dia::Maths::Vector2D& world) const;

			////////////////////////////////////////////////////////////
			/// \brief Return the axis-aligned bounding rectangle of the visible
			///        viewport expressed in world space.
			///
			/// For a rotated camera the returned rect is the AABB of the four
			/// visible screen corners projected into world space.
			///
			/// \return  AARect with min/max world-space corners of the visible area.
			////////////////////////////////////////////////////////////
			Dia::Geometry2D::AARect GetWorldBounds() const;

		private:
			Camera2D              mCamera;
			Dia::Maths::Vector2D  mWindowSize;
			float                 mCosAngle;  ///< cos(-rotation) precomputed
			float                 mSinAngle;  ///< sin(-rotation) precomputed
		};

	} // namespace Graphics
} // namespace Dia
