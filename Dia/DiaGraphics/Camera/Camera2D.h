////////////////////////////////////////////////////////////////////////////////
// Filename: Camera2D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		/// \brief Lightweight 2D camera value type.
		///
		/// Stores the world-space position the camera is centred on,
		/// a uniform zoom factor (>1 = zoomed in, world appears larger),
		/// and a clockwise rotation in degrees.
		///
		/// Trivially copyable — no heap allocation, no virtual functions.
		////////////////////////////////////////////////////////////
		class Camera2D
		{
		public:
			////////////////////////////////////////////////////////////
			/// \brief Default constructor — camera at origin, zoom 1, no rotation.
			////////////////////////////////////////////////////////////
			Camera2D()
				: mPosition(0.0f, 0.0f)
				, mZoom(1.0f)
				, mRotation(0.0f)
			{
			}

			////////////////////////////////////////////////////////////
			/// \brief Explicit constructor.
			///
			/// \param position  World-space centre point the camera looks at.
			/// \param zoom      Uniform zoom factor (default 1.0).
			/// \param rotation  Clockwise rotation in degrees (default 0.0).
			////////////////////////////////////////////////////////////
			explicit Camera2D(Dia::Maths::Vector2D position, float zoom = 1.0f, float rotation = 0.0f)
				: mPosition(position)
				, mZoom(zoom)
				, mRotation(rotation)
			{
			}

			////////////////////////////////////////////////////////////
			// Accessors
			////////////////////////////////////////////////////////////
			const Dia::Maths::Vector2D& GetPosition() const { return mPosition; }
			float                       GetZoom()      const { return mZoom; }
			float                       GetRotation()  const { return mRotation; }

			void SetPosition(const Dia::Maths::Vector2D& position) { mPosition = position; }
			void SetZoom(float zoom)                                { mZoom = zoom; }
			void SetRotation(float rotation)                        { mRotation = rotation; }

		private:
			Dia::Maths::Vector2D mPosition; ///< World-space centre the camera looks at
			float                mZoom;     ///< Uniform zoom (>1 = zoomed in)
			float                mRotation; ///< Clockwise rotation in degrees
		};

	} // namespace Graphics
} // namespace Dia
