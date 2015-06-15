#include "Vector/Vector4DAdapter.h"

#include "Vector/Vector2D.h"
#include "Vector/Vector3D.h"
#include "Vector/VectorUtils.h"

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		Vector4DAdapter::Vector4DAdapter( const Vector2D& vec )
		{
			VectorUtils::ToVector4DPointFromVector2D(mVector, vec);
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		Vector4DAdapter::Vector4DAdapter( const Vector3D& vec )
		{
			VectorUtils::ToVector4DPointFromVector3D(mVector, vec);
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		Vector4DAdapter::Vector4DAdapter( const Vector4D& vec )
		{
			mVector = vec;
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		const Vector4D& Vector4DAdapter::Data() const
		{
			return mVector;
		}
	}
}