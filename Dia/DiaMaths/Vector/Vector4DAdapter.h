#pragma once

#include "Vector/Vector4D.h"

namespace Dia
{
	namespace Maths
	{
		class Vector2D;
		class Vector3D;

		class Vector4DAdapter
		{
		public:
			Vector4DAdapter( const Vector2D& vec );
            Vector4DAdapter( const Vector3D& vec ); 
			Vector4DAdapter( const Vector4D& vec ); 
			const Vector4D& Data() const;

		private:
			Vector4D mVector;
		};
	}
}
