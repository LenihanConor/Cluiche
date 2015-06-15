#pragma once

#include "Vector/Vector3D.h"

namespace Dia
{
	namespace Maths
	{
		class Vector2D;
		class Vector4D;

		class Vector3DAdapter
		{
		public:
			Vector3DAdapter( const Vector2D& vec );
            Vector3DAdapter( const Vector3D& vec ); 
			Vector3DAdapter( const Vector4D& vec ); 
			const Vector3D& Data() const;

		private:
			Vector3D mVector;
		};
	}
}
