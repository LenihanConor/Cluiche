////////////////////////////////////////////////////////////////////////////////
// Filename: IRenderTarget.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaGraphics/Misc/RGBA.h"
#include "DiaGraphics/Misc/RenderStates.h"
#include "DiaGraphics/Misc/Vertex.h"
#include "DiaGraphics/Misc/PrimitiveType.h"

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGeometry2D/Shapes/AARect.h>

namespace Dia
{
	namespace Graphics
	{
		class IView;
		class IDrawable;

		////////////////////////////////////////////////////////////////////////////////
		// Enum name: IRenderTarget
		////////////////////////////////////////////////////////////////////////////////
		class IRenderTarget
		{
		public:
			IRenderTarget(){};

			virtual ~IRenderTarget(){};

			////////////////////////////////////////////////////////////
			/// Clear the entire target with a single color
			virtual void Clear(const RGBA& color = RGBA(0, 0, 0, 255)) = 0;

			////////////////////////////////////////////////////////////
			///  Change the current active view
			virtual void SetView(const IView& view) = 0;

			////////////////////////////////////////////////////////////
			/// Get the view currently in use in the render target
			virtual const IView& GetView() const = 0;

			////////////////////////////////////////////////////////////
			/// Get the default view of the render target
			virtual const IView& GetDefaultView() const = 0;

			////////////////////////////////////////////////////////////
			///  Get the viewport of a view, applied to this render target
			virtual Dia::Geometry2D::AARect GetViewport(const IView& view) const = 0;

			////////////////////////////////////////////////////////////
			/// Convert a point from target coordinates to world
			///        coordinates, using the current view
			virtual Maths::Vector2D MapPixelToCoords(const Maths::Vector2D& point) const = 0;

			////////////////////////////////////////////////////////////
			/// Convert a point from target coordinates to world coordinates
			virtual Maths::Vector2D MapPixelToCoords(const Maths::Vector2D& point, const IView& view) const = 0;

			////////////////////////////////////////////////////////////
			///  Convert a point from world coordinates to target
			virtual Maths::Vector2D MapCoordsToPixel(const Maths::Vector2D& point) const = 0;

			////////////////////////////////////////////////////////////
			///  Convert a point from world coordinates to target coordinates
			virtual Maths::Vector2D MapCoordsToPixel(const Maths::Vector2D& point, const IView& view) const = 0;

			////////////////////////////////////////////////////////////
			/// \brief Draw a drawable object to the render-target
			///
			/// \param drawable Object to draw
			/// \param states   Render states to use for drawing
			///
			////////////////////////////////////////////////////////////
			virtual void Draw(const IDrawable& drawable, const RenderStates& states = RenderStates::Default) = 0;

			////////////////////////////////////////////////////////////
			/// \brief Draw primitives defined by an array of vertices
			///
			/// \param vertices    Pointer to the vertices
			/// \param vertexCount Number of vertices in the array
			/// \param type        Type of primitives to draw
			/// \param states      Render states to use for drawing
			///
			////////////////////////////////////////////////////////////
			virtual void Draw(const Vertex* vertices, unsigned int vertexCount,
				PrimitiveType type, const RenderStates& states = RenderStates::Default) = 0;

			////////////////////////////////////////////////////////////
			/// \brief Return the size of the rendering region of the target
			virtual Maths::Vector2D GetRenderTargetSize() const = 0;

			////////////////////////////////////////////////////////////
			/// Save the current OpenGL render states and matrices
			virtual void PushGLStates() = 0;

			////////////////////////////////////////////////////////////
			/// \brief Restore the previously saved OpenGL render states and matrices
			virtual void PopGLStates() = 0;

			////////////////////////////////////////////////////////////
			/// Reset the internal OpenGL states so that the target is ready for drawing
			virtual void ResetGLStates() = 0;

		private:
			IRenderTarget(const IRenderTarget&);
			IRenderTarget& operator =(const IRenderTarget&);
		};
	}
}