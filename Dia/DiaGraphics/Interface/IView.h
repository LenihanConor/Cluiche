////////////////////////////////////////////////////////////////////////////////
// Filename: IView.h - 2D Camera
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace Maths
	{
		class Vector2D;
	}

	namespace Geometry2D
	{
		class AARect;
	}

	namespace Graphics
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: IView - 2D Camera
		////////////////////////////////////////////////////////////////////////////////
		class IView
		{
		public:
			IView(){}
			explicit IView(const Geometry2D::AARect& rectangle);
			explicit IView(const Maths::Vector2D& center, const Maths::Vector2D& size) = 0;

			virtual ~IView(){};
			
			virtual void SetCenter(float x, float y) = 0;
			virtual void SetCenter(const Maths::Vector2D& center) = 0;
			virtual void SetSize(float width, float height) = 0;
			virtual void SetSize(const Maths::Vector2D& size) = 0;
			virtual void SetRotation(float angle) = 0;
			virtual void SetViewport(const Geometry2D::AARect& viewport) = 0;

			virtual void Reset(const Geometry2D::AARect& rectangle) = 0;
			virtual const Maths::Vector2D& GetCenter() const = 0;
			virtual const Maths::Vector2D& GetSize() const = 0;
			virtual float GetRotation() const = 0;
			virtual const Geometry2D::AARect& GetViewport() const = 0;
			virtual void Translate(float offsetX, float offsetY) = 0;
			virtual void Translate(const Maths::Vector2D& offset) = 0;
			virtual void Rotate(float angle) = 0;
			virtual void Zoom(float factor) = 0;
		};
	}
}

		