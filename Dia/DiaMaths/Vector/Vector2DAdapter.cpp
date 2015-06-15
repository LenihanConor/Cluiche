#include "Vector/Vector2DAdapter.h"

#include "Vector/Vector3D.h"
#include "Vector/Vector4D.h"
#include "Vector/VectorUtils.h"

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		Vector2DAdapter::Vector2DAdapter( const Vector2D& vec )
		{
			mVector = vec;
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		Vector2DAdapter::Vector2DAdapter( const Vector3D& vec )
		{
			VectorUtils::ToVector2DFromVector3D(mVector, vec);
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		Vector2DAdapter::Vector2DAdapter( const Vector4D& vec )
		{
			VectorUtils::ToVector2DFromVector4D(mVector, vec);
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		const Vector2D& Vector2DAdapter::Data() const
		{
			return mVector;
		}
	}
}