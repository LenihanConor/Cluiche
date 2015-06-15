#pragma once

#include "Vector/Vector2D.h"

namespace Dia
{
	namespace Maths
	{
		class Vector3D;
		class Vector4D;

		class Vector2DAdapter
		{
		public:
			Vector2DAdapter( const Vector2D& vec );
            Vector2DAdapter( const Vector3D& vec ); 
			Vector2DAdapter( const Vector4D& vec ); 
			const Vector2D& Data() const;

		private:
			Vector2D mVector;
		};
	}
}
