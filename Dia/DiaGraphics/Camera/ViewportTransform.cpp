////////////////////////////////////////////////////////////////////////////////
// Filename: ViewportTransform.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Camera/ViewportTransform.h"

#include <cmath>

namespace Dia
{
	namespace Graphics
	{
		static const float kDegToRad = 3.14159265f / 180.0f;

		////////////////////////////////////////////////////////////
		ViewportTransform::ViewportTransform(const Camera2D& camera, const Dia::Maths::Vector2D& windowSize)
			: mCamera(camera)
			, mWindowSize(windowSize)
		{
			// Precompute rotation components.
			// A clockwise screen rotation requires rotating world points by -rotation,
			// so we store cos(-rotation) and sin(-rotation).
			const float rad = -camera.GetRotation() * kDegToRad;
			mCosAngle = cosf(rad);
			mSinAngle = sinf(rad);
		}

		////////////////////////////////////////////////////////////
		Dia::Maths::Vector2D ViewportTransform::WorldToScreen(const Dia::Maths::Vector2D& world) const
		{
			// Step 1 — translate relative to camera centre
			float px = world.x - mCamera.GetPosition().x;
			float py = world.y - mCamera.GetPosition().y;

			// Step 2 — rotate by -rotation (precomputed)
			float rx = px * mCosAngle - py * mSinAngle;
			float ry = px * mSinAngle + py * mCosAngle;

			// Step 3 — apply zoom
			const float zoom = mCamera.GetZoom();
			rx *= zoom;
			ry *= zoom;

			// Step 4 — translate to screen centre
			rx += mWindowSize.x * 0.5f;
			ry += mWindowSize.y * 0.5f;

			return Dia::Maths::Vector2D(rx, ry);
		}

		////////////////////////////////////////////////////////////
		Dia::Maths::Vector2D ViewportTransform::ScreenToWorld(const Dia::Maths::Vector2D& pixel) const
		{
			// Step 1 — translate from screen centre
			float px = pixel.x - mWindowSize.x * 0.5f;
			float py = pixel.y - mWindowSize.y * 0.5f;

			// Step 2 — undo zoom
			const float zoom = mCamera.GetZoom();
			px /= zoom;
			py /= zoom;

			// Step 3 — rotate by +rotation (inverse = transpose of rotation matrix)
			// cos(+rotation) = mCosAngle (cos is even), sin(+rotation) = -mSinAngle
			float rx = px *  mCosAngle + py * mSinAngle;
			float ry = px * -mSinAngle + py * mCosAngle;

			// Step 4 — translate by camera position
			rx += mCamera.GetPosition().x;
			ry += mCamera.GetPosition().y;

			return Dia::Maths::Vector2D(rx, ry);
		}

		////////////////////////////////////////////////////////////
		Dia::Geometry2D::AARect ViewportTransform::GetWorldBounds() const
		{
			const float w = mWindowSize.x;
			const float h = mWindowSize.y;

			// Project all four screen corners into world space
			const Dia::Maths::Vector2D corners[4] =
			{
				ScreenToWorld(Dia::Maths::Vector2D(0.0f, 0.0f)),
				ScreenToWorld(Dia::Maths::Vector2D(w,    0.0f)),
				ScreenToWorld(Dia::Maths::Vector2D(0.0f, h   )),
				ScreenToWorld(Dia::Maths::Vector2D(w,    h   ))
			};

			// Compute AABB of the four projected corners
			float minX = corners[0].x;
			float minY = corners[0].y;
			float maxX = corners[0].x;
			float maxY = corners[0].y;

			for (int i = 1; i < 4; ++i)
			{
				if (corners[i].x < minX) minX = corners[i].x;
				if (corners[i].y < minY) minY = corners[i].y;
				if (corners[i].x > maxX) maxX = corners[i].x;
				if (corners[i].y > maxY) maxY = corners[i].y;
			}

			return Dia::Geometry2D::AARect(
				Dia::Maths::Vector2D(minX, minY),
				Dia::Maths::Vector2D(maxX, maxY)
			);
		}

	} // namespace Graphics
} // namespace Dia
